#!/bin/bash

echo "=========================================="
echo "PHASE 7: End-to-End Integration Testing"
echo "=========================================="
echo ""

# Colors for output
GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Cleanup function
cleanup() {
    echo -e "\n${YELLOW}Cleaning up...${NC}"
    pkill -9 master 2>/dev/null
    pkill -9 backup 2>/dev/null
    pkill -9 node 2>/dev/null
    rm -f master.log backup.log gateway.log 2>/dev/null
    sleep 1
}

# Trap to cleanup on script exit
trap cleanup EXIT

# Start fresh
cleanup

echo -e "${YELLOW}Starting complete stack...${NC}\n"

# Step 1: Start Backup
echo "Step 1: Starting backup node on port 12346..."
cd /Users/achyutkatiyar/CS6650/FinalProject/backend
./backup 12346 1 127.0.0.1 12345 > backup.log 2>&1 &
BACKUP_PID=$!
sleep 1

if ps -p $BACKUP_PID > /dev/null; then
    echo -e "${GREEN}✓ Backup started (PID: $BACKUP_PID)${NC}"
else
    echo -e "${RED}✗ Backup failed to start${NC}"
    exit 1
fi
