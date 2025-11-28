#!/bin/bash

# Debug Replication Test - Isolates column and title replication issues

echo "=============================================="
echo "DEBUG REPLICATION TEST"
echo "=============================================="
echo ""

# Get paths
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
BACKEND_DIR="$PROJECT_ROOT/backend"
GATEWAY_DIR="$PROJECT_ROOT/gateway"

# Kill existing processes first
echo "Killing any existing processes..."
pkill -9 -f "master 12345" 2>/dev/null
pkill -9 -f "backup 12346" 2>/dev/null
lsof -ti:8080 | xargs kill -9 2>/dev/null
sleep 2

# Clear old logs
rm -f /tmp/master_debug.log /tmp/backup_debug.log /tmp/gateway_debug.log

echo "Step 1: Building backend..."
cd "$BACKEND_DIR"
make 2>&1 | tail -5
echo ""

echo "Step 2: Starting backup node on port 12346..."
./backup 12346 1 127.0.0.1 12345 > /tmp/backup_debug.log 2>&1 &
BACKUP_PID=$!
sleep 2

echo "Step 3: Starting master node on port 12345..."
./master 12345 0 127.0.0.1 12346 > /tmp/master_debug.log 2>&1 &
MASTER_PID=$!
sleep 3

echo "Step 4: Starting gateway on port 8080..."
cd "$GATEWAY_DIR"
node server.js > /tmp/gateway_debug.log 2>&1 &
GATEWAY_PID=$!
sleep 3

echo ""
echo "PIDs: Master=$MASTER_PID, Backup=$BACKUP_PID, Gateway=$GATEWAY_PID"
echo ""

echo "=============================================="
echo "Step 5: Creating 3 tasks in DIFFERENT columns"
echo "=============================================="
echo ""

# Task 1: TODO (column 0)
echo "Creating Task 1 in TODO (column=0)..."
curl -s -X POST http://localhost:8080/api/tasks \
  -H "Content-Type: application/json" \
  -d '{"title":"My First Task","description":"Desc 1","column":0,"created_by":"alice","board_id":"board-1"}'
echo ""
sleep 1

# Task 2: IN_PROGRESS (column 1)
echo "Creating Task 2 in IN_PROGRESS (column=1)..."
curl -s -X POST http://localhost:8080/api/tasks \
  -H "Content-Type: application/json" \
  -d '{"title":"Second Task","description":"Desc 2","column":1,"created_by":"bob","board_id":"board-1"}'
echo ""
sleep 1

# Task 3: DONE (column 2)
echo "Creating Task 3 in DONE (column=2)..."
curl -s -X POST http://localhost:8080/api/tasks \
  -H "Content-Type: application/json" \
  -d '{"title":"Third Task","description":"Desc 3","column":2,"created_by":"charlie","board_id":"board-1"}'
echo ""

sleep 2

echo ""
echo "=============================================="
echo "Step 6: Query MASTER for current state"
echo "=============================================="
echo ""
BOARD_BEFORE=$(curl -s http://localhost:8080/api/boards/board-1 2>/dev/null)
echo "$BOARD_BEFORE" | python3 -m json.tool 2>/dev/null || echo "Raw: $BOARD_BEFORE"

echo ""
echo "=== MASTER LOG ==="
cat /tmp/master_debug.log
echo ""
echo "=== BACKUP LOG (before kill) ==="
cat /tmp/backup_debug.log

echo ""
echo "=============================================="
echo "Step 7: KILLING MASTER"
echo "=============================================="
kill -9 $MASTER_PID 2>/dev/null
echo "Master killed!"
sleep 5

echo ""
echo "=== BACKUP LOG (after kill) ==="
cat /tmp/backup_debug.log

echo ""
echo "=============================================="
echo "Step 8: Query BACKUP (promoted) for state"
echo "=============================================="
echo ""
BOARD_AFTER=$(curl -s http://localhost:8080/api/boards/board-1 2>/dev/null)
echo "$BOARD_AFTER" | python3 -m json.tool 2>/dev/null || echo "Raw: $BOARD_AFTER"

echo ""
echo "=============================================="
echo "COMPARISON"
echo "=============================================="
echo ""
echo "BEFORE (from Master):"
echo "$BOARD_BEFORE" | python3 -c "
import sys, json
try:
    data = json.load(sys.stdin)
    for t in data.get('tasks', []):
        print(f\"  id={t.get('task_id')}, title='{t.get('title')}', column={t.get('column')}\")
except: pass
" 2>/dev/null

echo ""
echo "AFTER (from Backup):"
echo "$BOARD_AFTER" | python3 -c "
import sys, json
try:
    data = json.load(sys.stdin)
    for t in data.get('tasks', []):
        print(f\"  id={t.get('task_id')}, title='{t.get('title')}', column={t.get('column')}\")
except: pass
" 2>/dev/null

echo ""
echo "=============================================="
echo "EXPECTED:"
echo "  id=0, title='My First Task', column=0"
echo "  id=1, title='Second Task', column=1"
echo "  id=2, title='Third Task', column=2"
echo "=============================================="

echo ""
echo "Cleaning up..."
pkill -9 -f "backup 12346" 2>/dev/null
lsof -ti:8080 | xargs kill -9 2>/dev/null
echo "Done!"
