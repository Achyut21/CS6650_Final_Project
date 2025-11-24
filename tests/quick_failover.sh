#!/bin/bash

# Tests master failover to backup

echo "=========================================="
echo "Quick Failover Test"
echo "=========================================="
echo ""
echo "This will:"
echo "1. Start backup + master + gateway"
echo "2. Create 3 tasks"
echo "3. Forcefully kill master (kill -9)"
echo "4. Show backup promotion"
echo "5. Test creating task via backup"
echo ""

# Get script directory
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
BACKEND_DIR="$PROJECT_ROOT/backend"
GATEWAY_DIR="$PROJECT_ROOT/gateway"

# Kill everything first
pkill -9 master backup node 2>/dev/null
sleep 1

# Start backup
echo "Starting backup..."
cd "$BACKEND_DIR"
./backup 12346 1 127.0.0.1 12345 &
sleep 2

# Start master
echo "Starting master..."
./master 12345 0 127.0.0.1 12346 &
MASTER_PID=$!
sleep 2

# Start gateway
echo "Starting gateway..."
cd "$GATEWAY_DIR"
node server.js &
sleep 3

echo ""
echo "Creating 3 tasks..."
for i in {1..3}; do
  curl -s -X POST http://localhost:8080/api/tasks \
    -H "Content-Type: application/json" \
    -d "{\"title\":\"Task $i\",\"description\":\"Test\",\"column\":0,\"created_by\":\"test\",\"board_id\":\"board-1\"}" > /dev/null
  echo "  Task $i created"
done

sleep 2
echo ""
echo "Killing master with kill -9 (PID: $MASTER_PID)..."
kill -9 $MASTER_PID
echo "Master killed!"

echo ""
echo "Waiting 3 seconds for backup to detect and promote..."
sleep 3

echo ""
echo "Testing failover - creating task via backup..."
curl -X POST http://localhost:8080/api/tasks \
  -H "Content-Type: application/json" \
  -d '{"title":"After Failover","description":"Task via backup","column":0,"created_by":"test","board_id":"board-1"}'

echo ""
echo ""
echo "Check backup terminal for promotion messages!"
echo "Test complete."
