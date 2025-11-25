#!/bin/bash

echo "=========================================="
echo "Testing Heartbeat Mechanism"
echo "=========================================="
echo ""

# Cleanup
pkill -9 master backup node 2>/dev/null
sleep 1

echo "Starting backup on port 12346..."
cd /Users/achyutkatiyar/CS6650/FinalProject/backend
./backup 12346 1 127.0.0.1 12345 &
BACKUP_PID=$!
sleep 2

echo "Starting master with replication and heartbeat..."
./master 12345 0 127.0.0.1 12346 &
MASTER_PID=$!
sleep 3

echo ""
echo "Master and backup running."
echo "Watch for heartbeat messages every 5 seconds in master output."
echo ""
echo "You should see:"
echo "  [HEARTBEAT] Monitoring started"
echo "  [HEARTBEAT] 1/1 backups alive"
echo ""
echo "Press Ctrl+C to stop, or wait 20 seconds..."
sleep 20

echo ""
echo "Now killing backup to test failure detection..."
kill -9 $BACKUP_PID
echo "Backup killed - watch master detect failure on next heartbeat (within 5 seconds)"

sleep 10

echo ""
echo "Cleanup..."
kill -9 $MASTER_PID 2>/dev/null

echo ""
echo "Test complete!"
echo "Check master output for:"
echo "  [HEARTBEAT] WARNING: All backups disconnected!"
