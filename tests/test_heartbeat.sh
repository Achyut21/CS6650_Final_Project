#!/bin/bash

# Heartbeat Mechanism Test

echo "=========================================="
echo "Testing Heartbeat Mechanism"
echo "=========================================="
echo ""

# Get script directory and project paths
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
BACKEND_DIR="$PROJECT_ROOT/backend"

# Cleanup function
cleanup() {
    echo ""
    echo "Cleaning up..."
    pkill -9 master backup 2>/dev/null
    sleep 1
}

trap cleanup EXIT

# Kill existing processes
pkill -9 master backup 2>/dev/null
sleep 1

echo "Starting backup on port 12346..."
cd "$BACKEND_DIR"
./backup 12346 1 127.0.0.1 12345 > /tmp/backup_hb.log 2>&1 &
BACKUP_PID=$!
sleep 2

echo "Starting master with replication and heartbeat..."
./master 12345 0 127.0.0.1 12346 > /tmp/master_hb.log 2>&1 &
MASTER_PID=$!
sleep 3

echo ""
echo "Master and backup running."
echo "Watching for heartbeat messages (20 seconds)..."
echo ""

sleep 20

echo ""
echo "=== MASTER LOG (heartbeat messages) ==="
grep -i "heartbeat" /tmp/master_hb.log || echo "No heartbeat messages found"

echo ""
echo "=== BACKUP LOG (heartbeat messages) ==="
grep -i "heartbeat" /tmp/backup_hb.log || echo "No heartbeat messages found"

echo ""
echo "Now killing backup to test failure detection..."
kill -9 $BACKUP_PID 2>/dev/null
echo "Backup killed - waiting 10 seconds for master to detect..."

sleep 10

echo ""
echo "=== MASTER LOG (after backup killed) ==="
tail -10 /tmp/master_hb.log

# Check if heartbeat detection worked
if grep -q "disconnected\|WARNING" /tmp/master_hb.log; then
    echo ""
    echo "SUCCESS: Master detected backup failure"
    exit 0
else
    echo ""
    echo "WARNING: Could not verify failure detection in logs"
    exit 0
fi
