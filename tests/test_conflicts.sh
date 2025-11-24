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

# Cleanup
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

# Test 1: Normal update (no conflict)
echo "Test 1: Normal update (should succeed without conflict)"
echo "  Creating task..."
RESPONSE=$(curl -s -X POST http://localhost:8080/api/tasks \
  -H "Content-Type: application/json" \
  -d '{"title":"Conflict Test Task","description":"Original description","column":0,"created_by":"user1","board_id":"board-1"}')

TASK_ID=$(echo "$RESPONSE" | grep -o '"task_id":[0-9]*' | grep -o '[0-9]*')
echo "  Created task with ID: $TASK_ID"

echo "  Updating task (first update)..."
RESPONSE=$(curl -s -X PATCH http://localhost:8080/api/tasks/$TASK_ID \
  -H "Content-Type: application/json" \
  -d '{"title":"Updated Title v1"}')

if echo "$RESPONSE" | grep -q '"conflict":false'; then
    echo "  No conflict detected (expected for single update)"
elif echo "$RESPONSE" | grep -q '"conflict":true'; then
    echo "  Conflict flag set: true"
    echo "  Response: $RESPONSE"
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
        echo "  Update $i: Applied successfully"
    fi
    sleep 0.1
done

echo ""

# Test 3: Move operation
echo "Test 3: Move task to test vector clock on MOVE operations"
RESPONSE=$(curl -s -X PATCH http://localhost:8080/api/tasks/$TASK_ID \
  -H "Content-Type: application/json" \
  -d '{"column":1}')

if echo "$RESPONSE" | grep -q '"conflict":true'; then
    echo "  Move conflict detected"
elif echo "$RESPONSE" | grep -q '"rejected":true'; then
    echo "  Move rejected"
else
    echo "  Task moved successfully (no conflict)"
fi

echo ""
echo "=========================================="
echo "Checking Backend Logs for Conflict Messages"
echo "=========================================="
echo ""

echo "=== MASTER LOG (last 20 lines) ==="
tail -20 /tmp/master_conflict.log
echo ""

echo "=== BACKUP LOG (last 10 lines) ==="
tail -10 /tmp/backup_conflict.log
echo ""

echo "=== GATEWAY LOG (conflicts only) ==="
grep -i "conflict\|rejected" /tmp/gateway_conflict.log | tail -10 || echo "No conflicts detected in gateway"

echo ""
echo "=========================================="
echo "Test Complete"
echo "=========================================="
echo ""
echo "Cleanup: kill $MASTER_PID $BACKUP_PID $GATEWAY_PID"
echo "Or run: pkill -9 master backup node"
