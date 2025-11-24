# Test Scripts

This directory contains automated test scripts for the Distributed Project Management Platform.

## Prerequisites

All scripts must be run from this `tests/` directory:
```bash
cd FinalProject/tests
./script_name.sh
```

## Available Tests

### 1. test_replication.sh
Tests basic primary-backup replication.

**What it tests:**
- Backup starts and listens
- Master connects to backup
- Operations replicate to backup
- C++ client can create tasks

**Run:**
```bash
./test_replication.sh
```

**Expected output:**
- Master shows "Connected to backup"
- Backup shows "Replicated CREATE_TASK"
- Test client succeeds

---

### 2. quick_failover.sh
Quick test of master failover to backup.

**What it tests:**
- Master crash detection
- Backup promotion
- Gateway automatic failover
- Creating tasks via promoted backup

**Run:**
```bash
./quick_failover.sh
```

**Expected output:**
- 3 tasks created successfully
- Master killed
- Backup promotes: "PROMOTING TO MASTER"
- Task created via backup after failover

---

### 3. test_failover.sh
Comprehensive failover test with user confirmation.

**What it tests:**
- Same as quick_failover but with more tasks (5)
- Interactive (waits for Enter keypress)
- More detailed output

**Run:**
```bash
./test_failover.sh
```

---

### 4. test_conflicts.sh
Tests vector clock conflict detection.

**What it tests:**
- Normal updates (no conflict)
- Rapid sequential updates
- Conflict detection and reporting
- Gateway passes conflict info to client

**Run:**
```bash
./test_conflicts.sh
```

**Expected output:**
- Updates applied successfully
- Check logs for [CONFLICT] messages
- Responses include conflict flags

---

### 5. test_integration.sh
Full end-to-end integration test.

**What it tests:**
- All CRUD operations via REST API
- CREATE, UPDATE, MOVE, DELETE
- All components working together
- Replication during operations

**Run:**
```bash
./test_integration.sh
```

**Expected output:**
- All 4 CRUD operations succeed
- Component logs show activity
- No errors

---

## Test Output

All scripts write logs to `/tmp/`:
- `/tmp/master_*.log` - Master server output
- `/tmp/backup_*.log` - Backup server output
- `/tmp/gateway_*.log` - Gateway output

## Cleanup

All scripts automatically kill processes on exit.

Manual cleanup if needed:
```bash
pkill -9 master backup node
rm -f /tmp/master_*.log /tmp/backup_*.log /tmp/gateway_*.log
```

## Running All Tests

```bash
cd FinalProject/tests

# Run each test
./test_replication.sh
./test_integration.sh
./test_conflicts.sh
./quick_failover.sh
```

## Troubleshooting

**Port already in use:**
```bash
lsof -ti:12345 | xargs kill -9
lsof -ti:12346 | xargs kill -9
lsof -ti:8080 | xargs kill -9
```

**Scripts not executable:**
```bash
chmod +x *.sh
```

**curl not found:**
```bash
# Install curl (macOS)
brew install curl

# Install curl (Linux)
sudo apt-get install curl
```
