#!/bin/bash

# End to End Integration Test

echo "=========================================="
echo "End-to-End Integration Testing"
echo "=========================================="
echo ""

# Get script directory and project paths
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
BACKEND_DIR="$PROJECT_ROOT/backend"
GATEWAY_DIR="$PROJECT_ROOT/gateway"

# Cleanup function
cleanup() {
    echo ""
    echo "Cleaning up..."
    pkill -9 master 2>/dev/null
    pkill -9 backup 2>/dev/null
    pkill -9 node 2>/dev/null
    rm -f /tmp/master_int.log /tmp/backup_int.log /tmp/gateway_int.log 2>/dev/null
    sleep 1
}

# Trap to cleanup on script exit
trap cleanup EXIT

# Start fresh
cleanup

echo "Starting complete stack..."
echo ""

# Step 1: Start Backup
echo "Step 1: Starting backup node on port 12346..."
cd "$BACKEND_DIR"
./backup 12346 1 127.0.0.1 12345 > /tmp/backup_int.log 2>&1 &
BACKUP_PID=$!
sleep 1

if ps -p $BACKUP_PID > /dev/null; then
    echo "  Backup started (PID: $BACKUP_PID)"
else
    echo "  ERROR: Backup failed to start"
    exit 1
fi

# Step 2: Start Master
echo "Step 2: Starting master node with replication..."
./master 12345 0 127.0.0.1 12346 > /tmp/master_int.log 2>&1 &
MASTER_PID=$!
sleep 2

if ps -p $MASTER_PID > /dev/null; then
    echo "  Master started (PID: $MASTER_PID)"
else
    echo "  ERROR: Master failed to start"
    cat /tmp/master_int.log
    exit 1
fi

# Step 3: Start Gateway
echo "Step 3: Starting API Gateway..."
cd "$GATEWAY_DIR"
node server.js > /tmp/gateway_int.log 2>&1 &
GATEWAY_PID=$!
sleep 2

if ps -p $GATEWAY_PID > /dev/null; then
    echo "  Gateway started (PID: $GATEWAY_PID)"
else
    echo "  ERROR: Gateway failed to start"
    cat /tmp/gateway_int.log
    exit 1
fi

echo ""
echo "=========================================="
echo "All components running"
echo "=========================================="
echo ""

# Test 1: Create Task via REST API
echo "Test 1: Creating task via POST /api/tasks..."
RESPONSE=$(curl -s -X POST http://localhost:8080/api/tasks \
  -H "Content-Type: application/json" \
  -d '{"title":"Integration Test Task","description":"End-to-end test","column":0,"created_by":"test","board_id":"board-1"}')

if echo "$RESPONSE" | grep -q "task_id"; then
    echo "  Task created successfully"
    TASK_ID=$(echo "$RESPONSE" | grep -o '"task_id":[0-9]*' | grep -o '[0-9]*')
    echo "  Task ID: $TASK_ID"
else
    echo "  ERROR: Failed to create task"
    echo "  Response: $RESPONSE"
    exit 1
fi

# Test 2: Update Task
echo ""
echo "Test 2: Updating task via PATCH /api/tasks/$TASK_ID..."
RESPONSE=$(curl -s -X PATCH http://localhost:8080/api/tasks/$TASK_ID \
  -H "Content-Type: application/json" \
  -d '{"title":"Updated Title"}')

if echo "$RESPONSE" | grep -q "task_id"; then
    echo "  Task updated successfully"
    if echo "$RESPONSE" | grep -q '"conflict":true'; then
        echo "  WARNING: Conflict detected during update"
    fi
else
    echo "  ERROR: Failed to update task"
fi

# Test 3: Move Task
echo ""
echo "Test 3: Moving task via PATCH /api/tasks/$TASK_ID..."
RESPONSE=$(curl -s -X PATCH http://localhost:8080/api/tasks/$TASK_ID \
  -H "Content-Type: application/json" \
  -d '{"column":1}')

if echo "$RESPONSE" | grep -q "task_id"; then
    echo "  Task moved successfully"
else
    echo "  ERROR: Failed to move task"
fi

# Test 4: Delete Task
echo ""
echo "Test 4: Deleting task via DELETE /api/tasks/$TASK_ID..."
RESPONSE=$(curl -s -w "%{http_code}" -X DELETE http://localhost:8080/api/tasks/$TASK_ID)

if [ "$RESPONSE" = "204" ] || [ "$RESPONSE" = "200" ]; then
    echo "  Task deleted successfully"
else
    echo "  Response code: $RESPONSE"
fi

echo ""
echo "=========================================="
echo "Checking Component Logs"
echo "=========================================="
echo ""

echo "=== MASTER LOG (last 15 lines) ==="
tail -15 /tmp/master_int.log

echo ""
echo "=== BACKUP LOG (last 10 lines) ==="
tail -10 /tmp/backup_int.log

echo ""
echo "=========================================="
echo "Integration test complete"
echo "=========================================="
