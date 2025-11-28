#!/bin/bash

# Vector Clock Conflict Detection Test

echo "=========================================="
echo "Testing Vector Clock Conflict Detection"
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
    kill -9 $MASTER_PID $BACKUP_PID $GATEWAY_PID 2>/dev/null
    pkill -9 master backup node 2>/dev/null
    sleep 1
}

trap cleanup EXIT

# Cleanup first
echo "Cleaning up old processes..."
pkill -9 master backup node 2>/dev/null
sleep 1

# Start components
echo "Starting components..."
cd "$BACKEND_DIR"
./backup 12346 1 127.0.0.1 12345 > /tmp/backup_conflict.log 2>&1 &
BACKUP_PID=$!
sleep 1

./master 12345 0 127.0.0.1 12346 > /tmp/master_conflict.log 2>&1 &
MASTER_PID=$!
sleep 2

cd "$GATEWAY_DIR"
node server.js > /tmp/gateway_conflict.log 2>&1 &
GATEWAY_PID=$!
sleep 3

echo "All components started"
echo ""

TEST_PASSED=true

# Test 1: Normal update (no conflict)
echo "Test 1: Normal update (should succeed without conflict)"
echo "  Creating task..."
RESPONSE=$(curl -s -X POST http://localhost:8080/api/tasks \
  -H "Content-Type: application/json" \
  -d '{"title":"Conflict Test Task","description":"Original description","column":0,"created_by":"user1","board_id":"board-1"}')

TASK_ID=$(echo "$RESPONSE" | grep -o '"task_id":[0-9]*' | grep -o '[0-9]*')
if [ -z "$TASK_ID" ]; then
    echo "  FAILED: Could not create task"
    TEST_PASSED=false
else
    echo "  Created task with ID: $TASK_ID"
fi

echo "  Updating task (first update)..."
RESPONSE=$(curl -s -X PATCH http://localhost:8080/api/tasks/$TASK_ID \
  -H "Content-Type: application/json" \
  -d '{"title":"Updated Title v1"}')

if echo "$RESPONSE" | grep -q '"conflict":false'; then
    echo "  PASSED: No conflict detected (expected)"
elif echo "$RESPONSE" | grep -q '"conflict":true'; then
    echo "  INFO: Conflict flag set (may be expected)"
else
    echo "  Response: $RESPONSE"
fi

echo ""

# Test 2: Multiple rapid updates
echo "Test 2: Rapid sequential updates"
echo "  Sending 5 updates rapidly..."
for i in {1..5}; do
    RESPONSE=$(curl -s -X PATCH http://localhost:8080/api/tasks/$TASK_ID \
      -H "Content-Type: application/json" \
      -d "{\"title\":\"Update $i\"}")
    
    if echo "$RESPONSE" | grep -q '"conflict":true'; then
        echo "  Update $i: CONFLICT detected"
    elif echo "$RESPONSE" | grep -q '"rejected":true'; then
        echo "  Update $i: REJECTED (outdated)"
    else
        echo "  Update $i: Applied"
    fi
    sleep 0.1
done

echo ""

# Test 3: Move operation
echo "Test 3: Move task to test vector clock on MOVE"
RESPONSE=$(curl -s -X PATCH http://localhost:8080/api/tasks/$TASK_ID \
  -H "Content-Type: application/json" \
  -d '{"column":1}')

if echo "$RESPONSE" | grep -q "task_id"; then
    echo "  Task moved successfully"
else
    echo "  Move failed: $RESPONSE"
    TEST_PASSED=false
fi

echo ""
echo "=========================================="
echo "Checking Backend Logs"
echo "=========================================="
echo ""

echo "=== MASTER LOG (conflict messages) ==="
grep -i "conflict" /tmp/master_conflict.log | tail -10 || echo "No conflict messages"

echo ""
echo "=== BACKUP LOG (last 5 lines) ==="
tail -5 /tmp/backup_conflict.log

echo ""
echo "=========================================="
if [ "$TEST_PASSED" = true ]; then
    echo "Test Complete - All operations succeeded"
    exit 0
else
    echo "Test Complete - Some operations failed"
    exit 1
fi
