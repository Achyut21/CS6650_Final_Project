#!/bin/bash

# Test Both Master and Backup Rejoin Functionality

echo "=========================================="
echo "Master & Backup Rejoin Test"
echo "=========================================="
echo ""
echo "This test verifies:"
echo "1. Backup can rejoin and sync state from master"
echo "2. Master can rejoin and sync state from promoted backup"
echo ""

# Get script directory and project paths
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
BACKEND_DIR="$PROJECT_ROOT/backend"
GATEWAY_DIR="$PROJECT_ROOT/gateway"

# Cleanup function
cleanup() {
    pkill -9 master 2>/dev/null
    pkill -9 backup 2>/dev/null
    pkill -9 -f "node server.js" 2>/dev/null
    rm -f /tmp/master_rejoin.log /tmp/backup_rejoin.log /tmp/gateway_rejoin.log 2>/dev/null
    sleep 1
}

# Trap to cleanup on exit
trap cleanup EXIT

# Start fresh
cleanup

PASS=0
FAIL=0

pass() {
    echo "  ✓ $1"
    ((PASS++))
}

fail() {
    echo "  ✗ $1"
    ((FAIL++))
}

# ============================================
# PHASE 1: Initial Setup
# ============================================
echo "Phase 1: Starting initial cluster"
echo "-----------------------------------"

cd "$BACKEND_DIR"

# Start backup first
./backup 12346 1 127.0.0.1 12345 > /tmp/backup_rejoin.log 2>&1 &
BACKUP_PID=$!
sleep 1

if ps -p $BACKUP_PID > /dev/null 2>&1; then
    pass "Backup started (PID: $BACKUP_PID)"
else
    fail "Backup failed to start"
    exit 1
fi

# Start master
./master 12345 0 127.0.0.1 12346 > /tmp/master_rejoin.log 2>&1 &
MASTER_PID=$!
sleep 2

if ps -p $MASTER_PID > /dev/null 2>&1; then
    pass "Master started (PID: $MASTER_PID)"
else
    fail "Master failed to start"
    exit 1
fi

# Start gateway
cd "$GATEWAY_DIR"
node server.js > /tmp/gateway_rejoin.log 2>&1 &
GATEWAY_PID=$!
sleep 2

if ps -p $GATEWAY_PID > /dev/null 2>&1; then
    pass "Gateway started (PID: $GATEWAY_PID)"
else
    fail "Gateway failed to start"
    exit 1
fi

# Wait for master to connect to backup
sleep 3

# ============================================
# PHASE 2: Create Initial Tasks
# ============================================
echo ""
echo "Phase 2: Creating initial tasks"
echo "--------------------------------"

for i in 1 2 3; do
    RESPONSE=$(curl -s -X POST http://localhost:8080/api/tasks \
        -H "Content-Type: application/json" \
        -d "{\"title\":\"Task $i\",\"description\":\"Initial task $i\",\"column\":0,\"created_by\":\"test\",\"board_id\":\"board-1\"}")
    
    if echo "$RESPONSE" | grep -q "task_id"; then
        pass "Created Task $i"
    else
        fail "Failed to create Task $i"
    fi
done

sleep 2

# Verify tasks are replicated to backup
if grep -q "Replicated CREATE_TASK" /tmp/backup_rejoin.log; then
    pass "Tasks replicated to backup"
else
    fail "Tasks not replicated to backup"
fi

# ============================================
# PHASE 3: Test Backup Rejoin
# ============================================
echo ""
echo "Phase 3: Testing Backup Rejoin"
echo "-------------------------------"

echo "  Killing backup..."
kill -9 $BACKUP_PID 2>/dev/null
sleep 2

# Create one more task while backup is down
RESPONSE=$(curl -s -X POST http://localhost:8080/api/tasks \
    -H "Content-Type: application/json" \
    -d '{"title":"Task 4","description":"Created while backup down","column":0,"created_by":"test","board_id":"board-1"}')

if echo "$RESPONSE" | grep -q "task_id"; then
    pass "Created Task 4 (while backup down)"
else
    fail "Failed to create Task 4"
fi

sleep 1

# Restart backup
echo "  Restarting backup..."
cd "$BACKEND_DIR"
./backup 12346 1 127.0.0.1 12345 > /tmp/backup_rejoin2.log 2>&1 &
BACKUP_PID=$!
sleep 2

if ps -p $BACKUP_PID > /dev/null 2>&1; then
    pass "Backup restarted (PID: $BACKUP_PID)"
else
    fail "Backup failed to restart"
fi

# Wait for master's heartbeat to reconnect (up to 10 seconds)
echo "  Waiting for master to reconnect to backup..."
sleep 6

# Check if backup received state
if grep -q "\[REJOIN\] Received:" /tmp/backup_rejoin2.log; then
    TASKS_RECEIVED=$(grep "\[REJOIN\] Received:" /tmp/backup_rejoin2.log | grep -o "[0-9]* tasks" | head -1)
    pass "Backup rejoined and received $TASKS_RECEIVED"
else
    # Check if master reconnected via heartbeat
    if grep -q "REPLICATION_INIT" /tmp/backup_rejoin2.log; then
        pass "Backup reconnected via master heartbeat"
    else
        fail "Backup did not rejoin or reconnect"
    fi
fi

# Check master log for reconnection
if grep -q "\[RECONNECT\]" /tmp/master_rejoin.log || grep -q "1/1 backups alive" /tmp/master_rejoin.log; then
    pass "Master reconnected to backup"
else
    fail "Master did not reconnect to backup"
fi

# ============================================
# PHASE 4: Test Master Rejoin (after promotion)
# ============================================
echo ""
echo "Phase 4: Testing Master Rejoin"
echo "-------------------------------"

# First verify current task count
BOARD_RESPONSE=$(curl -s http://localhost:8080/api/boards/board-1)
TASK_COUNT=$(echo "$BOARD_RESPONSE" | grep -o '"task_id"' | wc -l | tr -d ' ')
echo "  Current task count: $TASK_COUNT"

# Kill master to trigger backup promotion
echo "  Killing master to trigger promotion..."
kill -9 $MASTER_PID 2>/dev/null
sleep 4

# Check backup promoted
if grep -q "PROMOTING TO MASTER" /tmp/backup_rejoin2.log; then
    pass "Backup promoted to master"
else
    fail "Backup did not promote"
fi

# Gateway should failover to backup
sleep 2

# Create task via promoted backup
RESPONSE=$(curl -s -X POST http://localhost:8080/api/tasks \
    -H "Content-Type: application/json" \
    -d '{"title":"Task 5","description":"Created on promoted backup","column":0,"created_by":"test","board_id":"board-1"}')

if echo "$RESPONSE" | grep -q "task_id"; then
    pass "Created Task 5 on promoted backup"
else
    fail "Failed to create task on promoted backup"
fi

sleep 1

# Now restart master - it should rejoin from promoted backup
echo "  Restarting master (should rejoin from promoted backup)..."
cd "$BACKEND_DIR"
./master 12345 0 127.0.0.1 12346 > /tmp/master_rejoin2.log 2>&1 &
MASTER_PID=$!
sleep 3

if ps -p $MASTER_PID > /dev/null 2>&1; then
    pass "Master restarted (PID: $MASTER_PID)"
else
    fail "Master failed to restart"
fi

# Check if master received state from promoted backup
if grep -q "\[REJOIN\]" /tmp/master_rejoin2.log; then
    TASKS_RECEIVED=$(grep "\[REJOIN\] Received:" /tmp/master_rejoin2.log | grep -o "[0-9]* tasks" | head -1)
    pass "Master rejoined and received $TASKS_RECEIVED"
else
    fail "Master did not rejoin from promoted backup"
fi

# Check if backup demoted
if grep -q "\[DEMOTE\]" /tmp/backup_rejoin2.log; then
    pass "Backup demoted back to backup mode"
else
    fail "Backup did not demote"
fi

# ============================================
# PHASE 5: Final Verification
# ============================================
echo ""
echo "Phase 5: Final Verification"
echo "----------------------------"

sleep 2

# Get final board state
BOARD_RESPONSE=$(curl -s http://localhost:8080/api/boards/board-1)
FINAL_COUNT=$(echo "$BOARD_RESPONSE" | grep -o '"task_id"' | wc -l | tr -d ' ')

if [ "$FINAL_COUNT" -eq 5 ]; then
    pass "All 5 tasks present after rejoin cycle"
else
    fail "Expected 5 tasks, got $FINAL_COUNT"
fi

# Create one more task to verify system is fully operational
RESPONSE=$(curl -s -X POST http://localhost:8080/api/tasks \
    -H "Content-Type: application/json" \
    -d '{"title":"Task 6","description":"Final verification","column":0,"created_by":"test","board_id":"board-1"}')

if echo "$RESPONSE" | grep -q "task_id"; then
    pass "System fully operational - created Task 6"
else
    fail "System not operational after rejoin"
fi

# ============================================
# Summary
# ============================================
echo ""
echo "=========================================="
echo "Test Summary"
echo "=========================================="
echo "Passed: $PASS"
echo "Failed: $FAIL"
echo ""

if [ $FAIL -eq 0 ]; then
    echo "All rejoin tests passed!"
    exit 0
else
    echo "Some tests failed. Check logs:"
    echo "  /tmp/master_rejoin.log"
    echo "  /tmp/master_rejoin2.log"
    echo "  /tmp/backup_rejoin.log"
    echo "  /tmp/backup_rejoin2.log"
    exit 1
fi
