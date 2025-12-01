# Distributed Project Management Platform

Real-time collaborative Kanban board with primary-backup replication for fault tolerance.

## Table of Contents

1. [Architecture Overview](#architecture-overview)
2. [Prerequisites](#prerequisites)
3. [Project Structure](#project-structure)
4. [Khoury Cluster Deployment](#khoury-cluster-deployment)
5. [Testing Guide](#testing-guide)
6. [Unit Tests](#unit-tests)
7. [Troubleshooting](#troubleshooting)
8. [Clean Shutdown](#clean-shutdown)
9. [Quick Reference](#quick-reference)
10. [Team Members](#team-members)
11. [Local Testing](#local-testing)

---

## Architecture Overview

```
┌─────────────────┐     HTTP/WS      ┌─────────────────┐      TCP        ┌─────────────────┐
│  React Frontend │ ◄──────────────► │  Node.js Gateway│ ◄─────────────► │   C++ Master    │
│   (Browser)     │    Port 8080     │   (server.js)   │   Port 12345    │   (master)      │
└─────────────────┘                  └─────────────────┘                 └────────┬────────┘
                                            │                                     │
                                            │ Failover                            │ Replication
                                            ▼                                     ▼
                                     ┌─────────────────┐                 ┌─────────────────┐
                                     │   C++ Backup    │ ◄───────────────│   Heartbeat     │
                                     │   (backup)      │   Port 12346    │   (5s interval) │
                                     └─────────────────┘                 └─────────────────┘
```

**Components:**
- **Frontend**: React + TypeScript + Tailwind CSS. Drag-and-drop via Atlaskit Pragmatic.
- **Gateway**: Express.js REST API + Socket.io WebSocket. Bridges HTTP to TCP binary protocol.
- **Master**: C++ server. Handles CRUD operations, replicates to backup.
- **Backup**: C++ hot standby. Promotes to master on primary failure. Supports master rejoin.

**Replication Protocol:**
- Master streams LogEntry objects over persistent TCP connection
- HEARTBEAT_PING/ACK every 5 seconds detects failures
- Backup promotes when master disconnects
- Rejoining master receives state transfer from promoted backup

## Prerequisites

- g++ with C++11 support
- Node.js 18+ and npm
- Access to Khoury Linux cluster (for deployment)
- Modern web browser

## Project Structure

```
.
├── backend/
│   ├── master.cpp          # Primary server
│   ├── backup.cpp          # Backup server
│   ├── task_manager.cpp    # Task storage with vector clocks
│   ├── state_machine.cpp   # Operation log for replication
│   ├── replication.cpp     # Heartbeat and log streaming
│   ├── messages.cpp        # Binary serialization
│   ├── Socket.cpp          # TCP wrapper
│   ├── ServerStub.cpp      # Server-side protocol
│   ├── ClientStub.cpp      # Client-side protocol
│   └── Makefile
├── gateway/
│   ├── server.js           # API gateway with failover
│   ├── package.json
│   └── public/             # Built frontend files
├── frontend/
│   ├── src/
│   │   ├── components/     # React components
│   │   ├── hooks/          # WebSocket hook
│   │   └── api/            # REST client
│   ├── package.json
│   └── vite.config.ts
└── build_frontend.sh       # Build script
```

## Khoury Cluster Deployment

> **IMPORTANT**: Master, Gateway, and SSH Tunnel all run on the SAME machine (<machine-1>).
> Only the Backup runs on a separate machine (<machine-2>).

### Find Machine IPs

SSH into any Khoury machine and get its IP:

```bash
ssh <username>@<machine-1>.khoury.northeastern.edu
hostname -i
# Example output: 10.200.125.81

# Example:
# ssh achyutk21@linux-081.khoury.northeastern.edu
# hostname -i  →  10.200.125.81
```

### Deployment Architecture

| Role    | Machine      | IP Example       | Port  |
|---------|--------------|------------------|-------|
| Master  | <machine-1>  | <ip-1>           | 12345 |
| Gateway | <machine-1>  | <ip-1>           | 8080  |
| Tunnel  | <machine-1>  | <ip-1>           | 8080  |
| Backup  | <machine-2>  | <ip-2>           | 12346 |

**Example with actual values:**

| Role    | Machine    | IP             | Port  |
|---------|------------|----------------|-------|
| Master  | linux-081  | 10.200.125.81  | 12345 |
| Gateway | linux-081  | 10.200.125.81  | 8080  |
| Tunnel  | linux-081  | 10.200.125.81  | 8080  |
| Backup  | linux-082  | 10.200.125.82  | 12346 |

### Step 1: Deploy Backend to Master Machine

```bash
# From local machine
ssh <username>@<machine-1>.khoury.northeastern.edu "mkdir -p <khoury-path>/backend"
scp -r <local-path>/backend/* <username>@<machine-1>:<khoury-path>/backend/

# Example:
# ssh achyutk21@linux-081.khoury.northeastern.edu "mkdir -p ~/project/backend"
# scp -r /Users/achyutkatiyar/CS6650/FinalProject/backend/* achyutk21@linux-081:~/project/backend/
```

Compile on the remote machine:

```bash
ssh <username>@<machine-1>.khoury.northeastern.edu
cd <khoury-path>/backend
make clean && make

# Example:
# ssh achyutk21@linux-081.khoury.northeastern.edu
# cd ~/project/backend && make clean && make
```

### Step 2: Deploy Backend to Backup Machine

```bash
# From local machine
ssh <username>@<machine-2>.khoury.northeastern.edu "mkdir -p <khoury-path>/backend"
scp -r <local-path>/backend/* <username>@<machine-2>:<khoury-path>/backend/

# Example:
# ssh achyutk21@linux-082.khoury.northeastern.edu "mkdir -p ~/project/backend"
# scp -r /Users/achyutkatiyar/CS6650/FinalProject/backend/* achyutk21@linux-082:~/project/backend/
```

Compile on the remote machine:

```bash
ssh <username>@<machine-2>.khoury.northeastern.edu
cd <khoury-path>/backend
make clean && make

# Example:
# ssh achyutk21@linux-082.khoury.northeastern.edu
# cd ~/project/backend && make clean && make
```

### Step 3: Deploy Gateway (on Master Machine)

```bash
# From local machine - deploy to SAME machine as master
ssh <username>@<machine-1>.khoury.northeastern.edu "mkdir -p <khoury-path>/gateway"
scp -r <local-path>/gateway/* <username>@<machine-1>:<khoury-path>/gateway/

# Example:
# ssh achyutk21@linux-081.khoury.northeastern.edu "mkdir -p ~/project/gateway"
# scp -r /Users/achyutkatiyar/CS6650/FinalProject/gateway/* achyutk21@linux-081:~/project/gateway/
```

Install dependencies:

```bash
ssh <username>@<machine-1>.khoury.northeastern.edu
cd <khoury-path>/gateway
npm install

# Example:
# ssh achyutk21@linux-081.khoury.northeastern.edu
# cd ~/project/gateway && npm install
```

### Step 4: Start Backup First (on Backup Machine)

On <machine-2> (separate SSH session):

```bash
cd <khoury-path>/backend
./backup 12346 1 <ip-1> 12345

# Example (on linux-082):
# cd ~/project/backend
# ./backup 12346 1 10.200.125.81 12345
```

The backup waits for master connection. Expected output:

```
Starting backup node 1 on port 12346
Primary: <ip-1>:12345
Starting fresh (master not reachable or no state to sync)
Backup listening on port 12346...
Waiting for primary connection or ready to promote...
```

### Step 5: Start Master (on Master Machine)

On <machine-1> (Terminal 1):

```bash
cd <khoury-path>/backend
./master 12345 0 <ip-2> 12346

# Example (on linux-081):
# cd ~/project/backend
# ./master 12345 0 10.200.125.82 12346
```

Expected output:

```
Starting master node 0 on port 12345
Connected to backup at <ip-2>:12346
Replication handshake successful with backup
Heartbeat monitoring started
Master listening on port 12345...
```

### Step 6: Start Gateway (on Master Machine)

On <machine-1> (Terminal 2 - same machine as master):

The gateway accepts machine number suffixes as arguments:

```bash
cd <khoury-path>/gateway
node server.js <master-suffix> <backup-suffix>

# Example (on linux-081):
# cd ~/project/gateway
# node server.js 81 82
```

Alternative: Edit server.js directly before starting:

```bash
# Update IPs in server.js
sed -i "s/const MASTER_HOST = '127.0.0.1'/const MASTER_HOST = '<ip-1>'/" server.js
sed -i "s/const BACKUP_HOST = '127.0.0.1'/const BACKUP_HOST = '<ip-2>'/" server.js
node server.js

# Example:
# sed -i "s/const MASTER_HOST = '127.0.0.1'/const MASTER_HOST = '10.200.125.81'/" server.js
# sed -i "s/const BACKUP_HOST = '127.0.0.1'/const BACKUP_HOST = '10.200.125.82'/" server.js
# node server.js
```

### Step 7: SSH Tunnel for Browser Access (to Master Machine)

On your **local machine** (laptop), open a new terminal:

```bash
ssh -L 8080:localhost:8080 <username>@<machine-1>.khoury.northeastern.edu

# Example:
# ssh -L 8080:localhost:8080 achyutk21@linux-081.khoury.northeastern.edu
```

Keep this terminal open. Access the app at: http://localhost:8080

### Summary: What Runs Where

```
<machine-1> (e.g., linux-081):
  ├── Master (./master 12345 0 <ip-2> 12346)     [Terminal 1]
  ├── Gateway (node server.js 81 82)             [Terminal 2]
  └── SSH Tunnel target (port 8080)

<machine-2> (e.g., linux-082):
  └── Backup (./backup 12346 1 <ip-1> 12345)     [Terminal 1]

Local Machine:
  └── SSH Tunnel (ssh -L 8080:localhost:8080 ...)
  └── Browser → http://localhost:8080
```


## Testing Guide

### Test 1: Basic CRUD Operations

Create 5 tasks:

```bash
# Create tasks
curl -X POST http://localhost:8080/api/tasks \
  -H "Content-Type: application/json" \
  -d '{"title":"Task 1","description":"First task","column":0,"board_id":"board-1","created_by":"Alice"}'

curl -X POST http://localhost:8080/api/tasks \
  -H "Content-Type: application/json" \
  -d '{"title":"Task 2","description":"Second task","column":0,"board_id":"board-1","created_by":"Alice"}'

curl -X POST http://localhost:8080/api/tasks \
  -H "Content-Type: application/json" \
  -d '{"title":"Task 3","description":"Third task","column":0,"board_id":"board-1","created_by":"Bob"}'

curl -X POST http://localhost:8080/api/tasks \
  -H "Content-Type: application/json" \
  -d '{"title":"Task 4","description":"Fourth task","column":1,"board_id":"board-1","created_by":"Bob"}'

curl -X POST http://localhost:8080/api/tasks \
  -H "Content-Type: application/json" \
  -d '{"title":"Task 5","description":"Fifth task","column":2,"board_id":"board-1","created_by":"Charlie"}'
```

Update 2 tasks:

```bash
curl -X PATCH http://localhost:8080/api/tasks/0 \
  -H "Content-Type: application/json" \
  -d '{"title":"Updated Task 1","description":"Modified description"}'

curl -X PATCH http://localhost:8080/api/tasks/1 \
  -H "Content-Type: application/json" \
  -d '{"title":"Updated Task 2"}'
```

Move 1 task:

```bash
curl -X PATCH http://localhost:8080/api/tasks/2 \
  -H "Content-Type: application/json" \
  -d '{"column":1}'
```

Delete 1 task:

```bash
curl -X DELETE http://localhost:8080/api/tasks/4
```

Verify final state:

```bash
curl http://localhost:8080/api/boards/board-1 | python3 -m json.tool
```

**Expected**: 4 tasks remain. Task 0 and 1 have updated titles. Task 2 is in column 1 (In Progress).

### Test 2: Real-Time Synchronization

1. Open http://localhost:8080 in two browser tabs (Tab A and Tab B)
2. In Tab A, click "+ New Task" and create a task
3. Observe Tab B

**Expected**: Task appears in Tab B within 200ms. Both tabs show identical state.

**Verification**: Open browser DevTools (F12) → Console. Look for:
```
WebSocket connected
```

### Test 3: Concurrent Edit Conflict Resolution

Run two updates simultaneously:

```bash
# Terminal 1: Alice updates task 0
curl -X PATCH http://localhost:8080/api/tasks/0 \
  -H "Content-Type: application/json" \
  -d '{"title":"Alice version"}' &

# Terminal 2: Bob updates task 0
curl -X PATCH http://localhost:8080/api/tasks/0 \
  -H "Content-Type: application/json" \
  -d '{"title":"Bob version"}' &

wait
```

Check result:

```bash
curl http://localhost:8080/api/boards/board-1 | python3 -m json.tool | grep -A2 '"task_id": 0'
```

**Expected**: Last write wins. One version persists. Check master logs for `[CONFLICT]` message.

### Test 4: Drag and Drop Task Movement

1. Open http://localhost:8080
2. Create a task in "To Do" column
3. Drag the task to "In Progress" column
4. Drag it to "Done" column

**Verification**:

```bash
curl http://localhost:8080/api/boards/board-1 | python3 -m json.tool
```

**Expected**: Task's `column` field shows 2 (Done). WebSocket broadcasts `TASK_MOVED` event.

### Test 5: Master Node Failure and Recovery

Setup: Ensure master and backup are running with tasks created.

1. Note current task count:

```bash
curl http://localhost:8080/api/boards/board-1 | python3 -c "import sys,json; print(len(json.load(sys.stdin)['tasks']))"
```

2. Kill master (Ctrl+C on master terminal)

3. Observe backup terminal. Expected output:

```
Primary disconnected
PROMOTING TO MASTER
Backup promoted! Now accepting client connections on port 12346
```

4. Gateway automatically fails over. Create a new task:

```bash
curl -X POST http://localhost:8080/api/tasks \
  -H "Content-Type: application/json" \
  -d '{"title":"After failover","description":"Created on backup","column":0,"board_id":"board-1","created_by":"Test"}'
```

5. Verify tasks preserved:

```bash
curl http://localhost:8080/api/boards/board-1 | python3 -m json.tool
```

**Expected**: All previous tasks present plus new task. Frontend reconnects automatically.

### Test 6: State Recovery from Log

1. With master running and tasks created, stop backup (Ctrl+C)

2. Restart backup:

```bash
./backup 12346 1 <ip-1> 12345
```

3. Observe backup output:

```
[REJOIN] Connected to master, requesting state sync
[REJOIN] Received: X tasks, Y log entries, ID counter: Z
[REJOIN] State applied successfully
```

4. Verify state matches:

```bash
# Check master task count (from master logs or via API)
# Check backup received same count
```

**Expected**: Backup reconstructs identical state to master.

### Test 7: Concurrent User Load

Run 10 parallel clients creating 5 tasks each:

```bash
for client in {1..10}; do
  for task in {1..5}; do
    curl -X POST http://localhost:8080/api/tasks \
      -H "Content-Type: application/json" \
      -d "{\"title\":\"Client${client}-Task${task}\",\"description\":\"Load test\",\"column\":0,\"board_id\":\"board-1\",\"created_by\":\"Client${client}\"}" &
  done
done
wait
```

Verify:

```bash
curl http://localhost:8080/api/boards/board-1 | python3 -c "import sys,json; print('Total tasks:', len(json.load(sys.stdin)['tasks']))"
```

**Expected**: 50 tasks created (plus any existing). No errors in master/gateway logs. No deadlocks.

### Test 8: Update Propagation Latency

1. Open 5 browser tabs connected to the board
2. Open DevTools Console in each tab
3. Add this to measure latency:

```javascript
const start = Date.now();
socket.on('TASK_CREATED', () => console.log('Latency:', Date.now() - start, 'ms'));
```

4. Create task via curl:

```bash
curl -X POST http://localhost:8080/api/tasks \
  -H "Content-Type: application/json" \
  -d '{"title":"Latency test","description":"Measuring propagation","column":0,"board_id":"board-1","created_by":"Tester"}'
```

**Expected**: 90th percentile latency under 200ms, 99th percentile under 500ms.


## Unit Tests

The backend includes comprehensive unit test suites. Running `make` builds all executables including the test binaries:

### Build All Tests

```bash
cd <local-path>/backend
make clean && make
```

### Available Test Suites

| Test Suite | Command | Description |
|------------|---------|-------------|
| Task Manager Tests | `./run_tests` | Tests VectorClock, Task, and TaskManager operations |
| State Machine Tests | `./run_sm_tests` | Tests operation log, replay, and state reconstruction |
| Marshalling Tests | `./run_marshal_tests` | Tests binary serialization for Task and LogEntry |
| Conflict Tests | `./run_conflict_tests` | Tests vector clock logic and conflict resolution |
| Network Tests | `./run_network_tests` | Tests TCP communication and stub protocols |

### Test Suite Details

**Task Manager Tests (`run_tests`)**
- VectorClock initialization, increment, update, and comparison
- Task creation, setters, and vector clock access
- TaskManager CRUD operations and workflow tests

**State Machine Tests (`run_sm_tests`)**
- Log append and retrieval operations
- Log replay with CREATE, UPDATE, MOVE, DELETE operations
- State reconstruction from operation log

**Marshalling Tests (`run_marshal_tests`)**
- Task marshal/unmarshal with various data types
- LogEntry serialization for all operation types
- Unicode support and edge cases (empty strings, max integers)
- Multiple marshal/unmarshal cycles

**Conflict Resolution Tests (`run_conflict_tests`)**
- Vector clock comparison (equal, greater, less, concurrent)
- Conflict detection with newer/older clocks
- Concurrent updates with last-write-wins resolution
- Multi-threaded conflict scenarios

**Network Protocol Tests (`run_network_tests`)**
- Socket bind, listen, connect, accept
- Data transfer (integers, strings, large payloads)
- Stub communication (Task, OpType, LogEntry)
- Heartbeat protocol and operation responses


## Troubleshooting

### Port Already in Use

```bash
# Find process using port
lsof -i :12345
lsof -i :12346
lsof -i :8080

# Kill process
kill -9 <PID>
```

### Compilation Errors

```bash
# Clean and rebuild
cd <khoury-path>/backend
make clean
make

# If pthread errors
g++ -std=c++11 -pthread -o master master.cpp ...
```

### Connection Refused

Check if services are running:

```bash
# On master machine
ps aux | grep master
ps aux | grep backup
ps aux | grep node

# Verify ports are listening
netstat -tlnp | grep -E '12345|12346|8080'
```

### SSH Tunnel Died

Reconnect the tunnel:

```bash
# Kill existing tunnel
pkill -f "ssh -L 8080"

# Reconnect
ssh -L 8080:localhost:8080 <username>@<machine-1>.khoury.northeastern.edu
```

### Gateway Can't Connect to Master

1. Verify master IP:

```bash
# On master machine
hostname -i
```

2. Check gateway configuration:

```bash
# In gateway/server.js, verify:
const MASTER_HOST = '<correct-ip>';
const MASTER_PORT = 12345;
```

3. Test connectivity:

```bash
# From gateway machine
nc -zv <ip-1> 12345
```

### WebSocket Not Connecting

1. Check CORS settings in gateway (already configured for `*`)

2. Verify gateway is serving frontend:

```bash
ls <khoury-path>/gateway/public/
# Should contain index.html and assets/
```

3. Check browser console for errors

4. Verify WebSocket endpoint:

```javascript
// In browser console
io('http://localhost:8080').on('connect', () => console.log('Connected'));
```

### Tasks Not Replicating

1. Check master logs for replication messages:

```
Connected to backup at <ip-2>:12346
Replication handshake successful
[HEARTBEAT] 1/1 backups alive
```

2. Check backup logs for received entries:

```
Replicated CREATE_TASK
Replicated MOVE_TASK
```

3. Verify backup connection:

```bash
# On master, look for heartbeat messages
[HEARTBEAT] 1/1 backups alive
```

### Failover Not Working

1. Verify backup is running and listening:

```bash
# On backup machine
ps aux | grep backup
netstat -tlnp | grep 12346
```

2. Check heartbeat timeout (5 seconds default)

3. Verify gateway failover configuration:

```javascript
// In server.js
const BACKUP_HOST = '<ip-2>';
const BACKUP_PORT = 12346;
```

4. Test direct connection to backup:

```bash
nc -zv <ip-2> 12346
```

### Master Rejoin Issues

When restarting master after backup promoted:

1. Master should detect promoted backup and receive state
2. Look for these logs on master:

```
[REJOIN] Backup was promoted, receiving state transfer
[REJOIN] State applied, next entry ID: X
```

3. And on backup:

```
[MASTER REJOIN] Master is rejoining
[STATE TRANSFER] Sending to master: X tasks
[DEMOTE] Backup demoted, returning to backup mode
```

## Clean Shutdown

Stop services in order:

```bash
# 1. Stop gateway (Ctrl+C or)
pkill -f "node server.js"

# 2. Stop master (Ctrl+C or)
pkill -f "./master"

# 3. Stop backup (Ctrl+C or)
pkill -f "./backup"
```

## Quick Reference

| Action | Command |
|--------|---------|
| Get all tasks | `curl http://localhost:8080/api/boards/board-1` |
| Create task | `curl -X POST http://localhost:8080/api/tasks -H "Content-Type: application/json" -d '{"title":"...","column":0,"board_id":"board-1","created_by":"..."}'` |
| Update task | `curl -X PATCH http://localhost:8080/api/tasks/<id> -H "Content-Type: application/json" -d '{"title":"..."}'` |
| Move task | `curl -X PATCH http://localhost:8080/api/tasks/<id> -H "Content-Type: application/json" -d '{"column":1}'` |
| Delete task | `curl -X DELETE http://localhost:8080/api/tasks/<id>` |
| Find IP | `hostname -i` |
| Check port | `netstat -tlnp \| grep <port>` |
| Kill port | `lsof -ti:<port> \| xargs kill -9` |

## Team Members

- Thomas Howes
- Achyut Katiyar

**Course**: CS6650 - Distributed Systems  
**Term**: Fall 2024

---

## Local Testing

### 1. Build Backend

```bash
cd <local-path>/backend
make clean && make
```

### 2. Start Master (Terminal 1)

```bash
./master 12345 0
```

### 3. Start Backup (Terminal 2)

```bash
./backup 12346 1 127.0.0.1 12345
```

### 4. Install Gateway Dependencies

```bash
cd <local-path>/gateway
npm install
```

### 5. Build Frontend

```bash
cd <local-path>/frontend
npm install
npm run build
cp -r dist/* ../gateway/public/
```

Or use the build script:

```bash
cd <local-path>
./build_frontend.sh
```

### 6. Start Gateway (Terminal 3)

```bash
cd <local-path>/gateway
node server.js
```

### 7. Access Application

Open browser: http://localhost:8080
