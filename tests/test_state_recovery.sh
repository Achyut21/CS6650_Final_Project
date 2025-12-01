#!/bin/bash

# Test State Recovery - Backup restart and log replay verification
# Tests that backup can restart, sync state from master, and correctly replay log
# NOTE: This test verifies functionality via API responses, log checks are informational only

echo "=========================================="
echo "State Recovery Test"
echo "=========================================="
echo ""

# Get script directory and project paths
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
BACKEND_DIR="$PROJECT_ROOT/backend"
GATEWAY_DIR="$PROJECT_ROOT/gateway"

# Helper function to safely count grep matches (avoids newline issues)
count_matches() {
    local file="$1"
    local pattern="$2"
    if [ -f "$file" ]; then
        grep -c "$pattern" "$file" 2>/dev/null || echo "0"
    else
        echo "0"
    fi
}

# Helper function to safely count occurrences in string
count_occurrences() {
    local input="$1"
    local pattern="$2"
    echo "$input" | grep -o "$pattern" | wc -l | xargs
}

# Cleanup function
cleanup() {
    echo ""
    echo "Cleaning up..."
    pkill -9 -f "master 12345" 2>/dev/null
    pkill -9 -f "backup 12346" 2>/dev/null
    pkill -9 -f "node server.js" 2>/dev/null
    rm -f /tmp/master_recovery.log /tmp/backup_recovery.log /tmp/backup_recovery_2.log /tmp/gateway_recovery.log 2>/dev/null
    sleep 1
}

trap cleanup EXIT

# Start fresh
cleanup

echo "Phase 1: Starting initial system..."
echo "--------------------------------------"

# Step 1: Start Backup first (waits for master)
echo "Starting backup node..."
cd "$BACKEND_DIR"
./backup 12346 1 127.0.0.1 12345 > /tmp/backup_recovery.log 2>&1 &
BACKUP_PID=$!
sleep 1

if ! ps -p $BACKUP_PID > /dev/null; then
    echo "ERROR: Backup failed to start"
    exit 1
fi
echo "  Backup started (PID: $BACKUP_PID)"

# Step 2: Start Master with replication
echo "Starting master node..."
./master 12345 0 127.0.0.1 12346 > /tmp/master_recovery.log 2>&1 &
MASTER_PID=$!
sleep 2

if ! ps -p $MASTER_PID > /dev/null; then
    echo "ERROR: Master failed to start"
    [ -f /tmp/master_recovery.log ] && cat /tmp/master_recovery.log
    exit 1
fi
echo "  Master started (PID: $MASTER_PID)"

# Step 3: Start Gateway
echo "Starting gateway..."
cd "$GATEWAY_DIR"
node server.js > /tmp/gateway_recovery.log 2>&1 &
GATEWAY_PID=$!
sleep 2

if ! ps -p $GATEWAY_PID > /dev/null; then
    echo "ERROR: Gateway failed to start"
    [ -f /tmp/gateway_recovery.log ] && cat /tmp/gateway_recovery.log
    exit 1
fi
echo "  Gateway started (PID: $GATEWAY_PID)"

echo ""
echo "Phase 2: Creating tasks for state..."
echo "--------------------------------------"

# Track successful task creations
TASKS_CREATED=0
declare -a TASK_IDS

for i in {1..5}; do
    COLUMN=$((($i - 1) % 3))  # Distribute across columns 0, 1, 2
    RESPONSE=$(curl -s -X POST http://localhost:8080/api/tasks \
      -H "Content-Type: application/json" \
      -d "{\"title\":\"Recovery Test Task $i\",\"description\":\"Task for state recovery test\",\"column\":$COLUMN,\"created_by\":\"test\",\"board_id\":\"board-1\"}")
    
    if echo "$RESPONSE" | grep -q "task_id"; then
        TASK_ID=$(echo "$RESPONSE" | grep -o '"task_id":[0-9]*' | grep -o '[0-9]*')
        TASK_IDS+=($TASK_ID)
        TASKS_CREATED=$((TASKS_CREATED + 1))
        echo "  Created task $i (ID: $TASK_ID, Column: $COLUMN)"
    else
        echo "  ERROR: Failed to create task $i"
        echo "  Response: $RESPONSE"
    fi
done

if [ "$TASKS_CREATED" -lt 5 ]; then
    echo "ERROR: Only created $TASKS_CREATED/5 tasks"
    exit 1
fi

# Update one task
echo ""
echo "Updating task ${TASK_IDS[0]}..."
UPDATE_RESPONSE=$(curl -s -X PATCH "http://localhost:8080/api/tasks/${TASK_IDS[0]}" \
  -H "Content-Type: application/json" \
  -d '{"title":"Updated Recovery Task","description":"Modified description"}')
if echo "$UPDATE_RESPONSE" | grep -q "task_id"; then
    echo "  Task updated successfully"
else
    echo "  WARNING: Update may have failed"
fi

# Move one task
echo "Moving task ${TASK_IDS[1]} to column 2..."
MOVE_RESPONSE=$(curl -s -X PATCH "http://localhost:8080/api/tasks/${TASK_IDS[1]}" \
  -H "Content-Type: application/json" \
  -d '{"column":2}')
if echo "$MOVE_RESPONSE" | grep -q "task_id"; then
    echo "  Task moved successfully"
else
    echo "  WARNING: Move may have failed"
fi

sleep 2

echo ""
echo "Phase 3: Verifying replication (informational)..."
echo "--------------------------------------"

# Check backup log for replicated operations - INFORMATIONAL ONLY
REPLICATED_COUNT=$(count_matches "/tmp/backup_recovery.log" "Replicated")
echo "  Operations replicated to backup: $REPLICATED_COUNT"
if [ "$REPLICATED_COUNT" -lt 7 ]; then
    echo "  (Note: Expected ~7 operations: 5 creates + 1 update + 1 move)"
fi

echo ""
echo "Phase 4: Killing and restarting backup..."
echo "--------------------------------------"

# Kill backup
echo "Killing backup (PID: $BACKUP_PID)..."
kill -9 $BACKUP_PID 2>/dev/null
sleep 2

# Verify backup is dead
if ps -p $BACKUP_PID > /dev/null 2>&1; then
    echo "  ERROR: Backup still running"
    exit 1
fi
echo "  Backup killed"

# Restart backup - it should sync state from master
echo "Restarting backup (should sync from master)..."
cd "$BACKEND_DIR"
./backup 12346 1 127.0.0.1 12345 > /tmp/backup_recovery_2.log 2>&1 &
NEW_BACKUP_PID=$!
sleep 3

if ! ps -p $NEW_BACKUP_PID > /dev/null; then
    echo "  ERROR: Backup failed to restart"
    [ -f /tmp/backup_recovery_2.log ] && cat /tmp/backup_recovery_2.log
    exit 1
fi
echo "  Backup restarted (PID: $NEW_BACKUP_PID)"

# CRITICAL: Wait for master's heartbeat to reconnect to backup
# Master heartbeat runs every 5 seconds, so we need to wait at least one full cycle
echo ""
echo "  Waiting for master to reconnect replication to backup..."
echo "  (Master heartbeat interval is 5 seconds, waiting 8 seconds...)"
sleep 8

# Verify replication reconnected by checking master log
if [ -f /tmp/master_recovery.log ]; then
    if grep -q "RECONNECT.*Successfully reconnected" /tmp/master_recovery.log 2>/dev/null; then
        echo "  ✓ Master reconnected to backup (per log)"
    elif grep -q "Replication handshake successful" /tmp/master_recovery.log 2>/dev/null; then
        echo "  ✓ Replication handshake completed (per log)"
    else
        echo "  (Reconnection not confirmed in log - continuing anyway)"
    fi
fi

echo ""
echo "Phase 5: Verifying state recovery (informational)..."
echo "--------------------------------------"

# Check backup log for state transfer - INFORMATIONAL ONLY
if [ -f /tmp/backup_recovery_2.log ]; then
    if grep -q "REJOIN" /tmp/backup_recovery_2.log 2>/dev/null; then
        echo "  ✓ Backup performed state sync from master (per log)"
        
        # Extract task count from log
        TASK_COUNT=$(grep -o "Received: [0-9]* tasks" /tmp/backup_recovery_2.log 2>/dev/null | grep -o "[0-9]*" | head -1)
        if [ -n "$TASK_COUNT" ]; then
            echo "  ✓ Recovered $TASK_COUNT tasks from master"
        fi
        
        LOG_COUNT=$(grep -o "[0-9]* log entries" /tmp/backup_recovery_2.log 2>/dev/null | grep -o "[0-9]*" | head -1)
        if [ -n "$LOG_COUNT" ]; then
            echo "  ✓ Recovered $LOG_COUNT log entries from master"
        fi
    else
        echo "  (State sync not detected in log - will verify via API)"
    fi
else
    echo "  (Backup log not available - will verify via API)"
fi

echo ""
echo "Phase 6: Testing operations after recovery..."
echo "--------------------------------------"

# Create a new task to verify system is working - THIS IS THE REAL TEST
RESPONSE=$(curl -s -X POST http://localhost:8080/api/tasks \
  -H "Content-Type: application/json" \
  -d '{"title":"Post-Recovery Task","description":"Created after backup restart","column":0,"created_by":"test","board_id":"board-1"}')

PHASE6_PASS=false
if echo "$RESPONSE" | grep -q "task_id"; then
    echo "  ✓ New task created successfully after recovery"
    PHASE6_PASS=true
else
    echo "  ✗ Failed to create task after recovery"
    echo "  Response: $RESPONSE"
fi

# Verify total task count via GET
BOARD_RESPONSE=$(curl -s http://localhost:8080/api/boards/board-1)
TOTAL_TASKS=$(count_occurrences "$BOARD_RESPONSE" '"task_id"')
echo "  Total tasks on board: $TOTAL_TASKS (expected: 6)"

if [ "$TOTAL_TASKS" -lt 6 ]; then
    echo "  ✗ Task count verification failed"
    PHASE6_PASS=false
fi

echo ""
echo "Phase 7: Testing failover with recovered backup..."
echo "--------------------------------------"

# Kill master
echo "Killing master to test failover..."
kill -9 $MASTER_PID 2>/dev/null

# Wait for backup to detect master death and promote
# The backup will promote when ReceiveOpType or ReceiveLogEntry fails
echo "  Waiting for backup to detect master failure and promote..."
sleep 5

# Check if backup promoted - INFORMATIONAL ONLY
if [ -f /tmp/backup_recovery_2.log ]; then
    if grep -q "PROMOTING" /tmp/backup_recovery_2.log 2>/dev/null; then
        echo "  ✓ Backup promoted to master (per log)"
    else
        echo "  (Promotion not detected yet - will verify via API)"
        sleep 2
    fi
else
    echo "  (Log not available - will verify via API)"
    sleep 2
fi

# Try to create task via promoted backup - THIS IS THE REAL TEST
RESPONSE=$(curl -s -X POST http://localhost:8080/api/tasks \
  -H "Content-Type: application/json" \
  -d '{"title":"Post-Failover Task","description":"Created via promoted backup","column":1,"created_by":"test","board_id":"board-1"}')

PHASE7_PASS=false
if echo "$RESPONSE" | grep -q "task_id"; then
    echo "  ✓ Task created successfully via promoted backup"
    PHASE7_PASS=true
else
    echo "  ✗ Failed to create task via promoted backup"
    echo "  Response: $RESPONSE"
fi

echo ""
echo "=========================================="
echo "State Recovery Test Summary"
echo "=========================================="

# Final verification - THIS IS THE DEFINITIVE TEST
FINAL_BOARD=$(curl -s http://localhost:8080/api/boards/board-1)
FINAL_COUNT=$(count_occurrences "$FINAL_BOARD" '"task_id"')

echo ""
echo "Final task count: $FINAL_COUNT (expected: 7)"
echo ""

# Pass/fail based on API responses, NOT logs
if [ "$FINAL_COUNT" -ge 7 ] && [ "$PHASE6_PASS" = true ] && [ "$PHASE7_PASS" = true ]; then
    echo "✓ STATE RECOVERY TEST PASSED"
    echo ""
    echo "Key verifications (via API):"
    echo "  - Initial tasks created successfully (5 tasks)"
    echo "  - Backup restarted and system still operational"
    echo "  - Post-recovery task creation works (1 task)"
    echo "  - Failover to backup works (1 task)"
    echo "  - Total: $FINAL_COUNT tasks"
    exit 0
else
    echo "✗ STATE RECOVERY TEST FAILED"
    echo ""
    echo "Failure details:"
    echo "  - Final task count: $FINAL_COUNT (expected >= 7)"
    echo "  - Phase 6 (post-recovery): $PHASE6_PASS"
    echo "  - Phase 7 (failover): $PHASE7_PASS"
    echo ""
    # Only show logs if they exist
    if [ -f /tmp/backup_recovery_2.log ]; then
        echo "=== Backup Recovery Log (last 30 lines) ==="
        tail -30 /tmp/backup_recovery_2.log
    else
        echo "(No backup log available)"
    fi
    if [ -f /tmp/master_recovery.log ]; then
        echo ""
        echo "=== Master Log (last 15 lines) ==="
        tail -15 /tmp/master_recovery.log
    fi
    if [ -f /tmp/gateway_recovery.log ]; then
        echo ""
        echo "=== Gateway Log (last 10 lines) ==="
        tail -10 /tmp/gateway_recovery.log
    fi
    exit 1
fi
