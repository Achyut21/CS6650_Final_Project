#!/bin/bash

# Quick Failover Test - Non-interactive version

echo "=========================================="
echo "Quick Failover Test"
echo "=========================================="
echo ""

# Get script directory
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
BACKEND_DIR="$PROJECT_ROOT/backend"
GATEWAY_DIR="$PROJECT_ROOT/gateway"

# Cleanup function
cleanup() {
    pkill -9 master backup node 2>/dev/null
    sleep 1
}

trap cleanup EXIT

# Kill everything first
cleanup

# Start backup
echo "Starting backup..."
cd "$BACKEND_DIR"
./backup 12346 1 127.0.0.1 12345 > /tmp/backup_quick.log 2>&1 &
BACKUP_PID=$!
sleep 2

# Start master
echo "Starting master..."
./master 12345 0 127.0.0.1 12346 > /tmp/master_quick.log 2>&1 &
MASTER_PID=$!
sleep 2

# Start gateway
echo "Starting gateway..."
cd "$GATEWAY_DIR"
node server.js > /tmp/gateway_quick.log 2>&1 &
GATEWAY_PID=$!
sleep 3

echo ""
echo "Creating 3 tasks..."
for i in {1..3}; do
  RESPONSE=$(curl -s -X POST http://localhost:8080/api/tasks \
    -H "Content-Type: application/json" \
    -d "{\"title\":\"Task $i\",\"description\":\"Test\",\"column\":0,\"created_by\":\"test\",\"board_id\":\"board-1\"}")
  if echo "$RESPONSE" | grep -q "task_id"; then
    echo "  Task $i created"
  else
    echo "  Task $i FAILED"
  fi
done

sleep 2
echo ""
echo "Killing master (PID: $MASTER_PID)..."
kill -9 $MASTER_PID 2>/dev/null

echo "Waiting 3 seconds for backup to promote..."
sleep 3

echo ""
echo "Testing failover - creating task via backup..."
RESPONSE=$(curl -s -X POST http://localhost:8080/api/tasks \
  -H "Content-Type: application/json" \
  -d '{"title":"After Failover","description":"Task via backup","column":0,"created_by":"test","board_id":"board-1"}')

echo ""
if echo "$RESPONSE" | grep -q "task_id"; then
    echo "SUCCESS: Failover worked!"
    echo "Response: $RESPONSE"
    echo ""
    echo "=== Backup promotion log ==="
    grep -i "promot\|MASTER" /tmp/backup_quick.log | tail -5
    exit 0
else
    echo "FAILED: Could not create task after failover"
    echo "Response: $RESPONSE"
    echo ""
    echo "=== Backup log ==="
    tail -10 /tmp/backup_quick.log
    exit 1
fi
