#!/bin/bash

# Master Failover Test

echo "=========================================="
echo "Testing Master Failover"
echo "=========================================="
echo ""

echo "This test will:"
echo "1. Start backup and master with replication"
echo "2. Create 5 tasks via gateway"
echo "3. Kill the master"
echo "4. Verify backup promotes itself"
echo "5. Gateway automatically fails over to backup"
echo "6. Create new task via backup (now promoted)"
echo ""
echo "Press Enter to start..."
read

# Get script directory and project paths
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
BACKEND_DIR="$PROJECT_ROOT/backend"
GATEWAY_DIR="$PROJECT_ROOT/gateway"

# Cleanup
pkill -9 master backup node 2>/dev/null
sleep 1

echo ""
echo "Step 1: Starting backup on port 12346..."
cd "$BACKEND_DIR"
./backup 12346 1 127.0.0.1 12345 &
BACKUP_PID=$!
sleep 2

echo "Step 2: Starting master with replication..."
./master 12345 0 127.0.0.1 12346 &
MASTER_PID=$!
sleep 2

echo "Step 3: Starting gateway..."
cd "$GATEWAY_DIR"
node server.js &
GATEWAY_PID=$!
sleep 3

echo ""
echo "Step 4: Creating 5 tasks via REST API..."
for i in {1..5}; do
    curl -s -X POST http://localhost:8080/api/tasks \
      -H "Content-Type: application/json" \
      -d "{\"title\":\"Task $i\",\"description\":\"Test task $i\",\"column\":0,\"created_by\":\"test\",\"board_id\":\"board-1\"}" > /dev/null
    echo "  Created task $i"
    sleep 0.5
done

echo ""
echo "Step 5: Waiting 2 seconds for replication to complete..."
sleep 2

echo ""
echo "Step 6: KILLING MASTER (simulating crash)..."
kill -9 $MASTER_PID
echo "  Master killed (PID: $MASTER_PID)"

echo ""
echo "Step 7: Waiting for backup to detect failure and promote (15 seconds)..."
sleep 15

echo ""
echo "Step 8: Testing if backup is now accepting requests..."
echo "  Creating task via backup (should auto-failover)..."

RESPONSE=$(curl -s -X POST http://localhost:8080/api/tasks \
  -H "Content-Type: application/json" \
  -d '{"title":"After Failover","description":"This task created after master died","column":0,"created_by":"test","board_id":"board-1"}')

if echo "$RESPONSE" | grep -q "task_id"; then
    echo "  SUCCESS: Gateway failed over to backup"
    echo "  Response: $RESPONSE"
else
    echo "  FAILED: Could not create task after failover"
    echo "  Response: $RESPONSE"
fi

echo ""
echo "=========================================="
echo "Failover test complete"
echo "=========================================="
echo ""
echo "Cleanup: kill $BACKUP_PID $GATEWAY_PID"
