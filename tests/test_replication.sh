#!/bin/bash

# Primary Backup Replication Test

echo "==================================="
echo "Testing Primary-Backup Replication"
echo "==================================="
echo ""

# Get script directory and project paths
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
BACKEND_DIR="$PROJECT_ROOT/backend"

# Kill any existing processes
echo "Cleaning up old processes..."
pkill -9 master 2>/dev/null
pkill -9 backup 2>/dev/null
sleep 1

# Start backup in background
echo ""
echo "Step 1: Starting backup node..."
cd "$BACKEND_DIR"
./backup 12346 1 127.0.0.1 12345 > /tmp/backup_test.log 2>&1 &
BACKUP_PID=$!
sleep 2

# Start master with replication enabled
echo "Step 2: Starting master with replication..."
./master 12345 0 127.0.0.1 12346 > /tmp/master_test.log 2>&1 &
MASTER_PID=$!
sleep 2

# Test with C++ client
echo "Step 3: Creating task via C++ client..."
./test_client

echo ""
echo "Step 4: Checking logs..."
echo ""
echo "=== MASTER LOG ==="
cat /tmp/master_test.log
echo ""
echo "=== BACKUP LOG ==="
cat /tmp/backup_test.log

echo ""
echo "Step 5: Cleanup..."
kill $MASTER_PID 2>/dev/null
kill $BACKUP_PID 2>/dev/null

echo ""
echo "Test complete"
