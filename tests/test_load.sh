#!/bin/bash

# Test 7: Concurrent User Load Test
# 10 clients, each creates 5 tasks over 30 seconds (50 total tasks)
# Verifies: All operations succeed, no deadlocks, measures latency

echo "=========================================="
echo "Test 7: Concurrent User Load Test"
echo "=========================================="
echo ""
echo "Specification:"
echo "  - 10 concurrent clients"
echo "  - Each client creates 5 tasks"
echo "  - Expected: 50 total tasks created"
echo "  - Measure operation latency"
echo "  - Verify no deadlocks or errors"
echo ""

# Get script directory and project paths
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
BACKEND_DIR="$PROJECT_ROOT/backend"
GATEWAY_DIR="$PROJECT_ROOT/gateway"

# Configuration
NUM_CLIENTS=10
TASKS_PER_CLIENT=5
TOTAL_EXPECTED=$((NUM_CLIENTS * TASKS_PER_CLIENT))

# Temp files for results
RESULTS_DIR="/tmp/load_test_$$"
mkdir -p "$RESULTS_DIR"

# Export for subshells
export RESULTS_DIR TASKS_PER_CLIENT

# Cleanup function
cleanup() {
    echo ""
    echo "Cleaning up..."
    pkill -9 -f "master 12345" 2>/dev/null
    pkill -9 -f "backup 12346" 2>/dev/null
    pkill -9 -f "node server.js" 2>/dev/null
    rm -f /tmp/master_load.log /tmp/backup_load.log /tmp/gateway_load.log 2>/dev/null
    sleep 1
}

trap 'cleanup; rm -rf "$RESULTS_DIR"' EXIT

# Start fresh (don't call full cleanup, just kill processes)
pkill -9 -f "master 12345" 2>/dev/null
pkill -9 -f "backup 12346" 2>/dev/null
pkill -9 -f "node server.js" 2>/dev/null
sleep 1

echo "Step 1: Starting system components..."
echo ""

# Start Backup
cd "$BACKEND_DIR"
./backup 12346 1 127.0.0.1 12345 > /tmp/backup_load.log 2>&1 &
BACKUP_PID=$!
sleep 1

if ! ps -p $BACKUP_PID > /dev/null 2>&1; then
    echo "ERROR: Backup failed to start"
    exit 1
fi
echo "  ✓ Backup started (PID: $BACKUP_PID)"

# Start Master
./master 12345 0 127.0.0.1 12346 > /tmp/master_load.log 2>&1 &
MASTER_PID=$!
sleep 2

if ! ps -p $MASTER_PID > /dev/null 2>&1; then
    echo "ERROR: Master failed to start"
    cat /tmp/master_load.log
    exit 1
fi
echo "  ✓ Master started (PID: $MASTER_PID)"

# Start Gateway
cd "$GATEWAY_DIR"
node server.js > /tmp/gateway_load.log 2>&1 &
GATEWAY_PID=$!
sleep 2

if ! ps -p $GATEWAY_PID > /dev/null 2>&1; then
    echo "ERROR: Gateway failed to start"
    cat /tmp/gateway_load.log
    exit 1
fi
echo "  ✓ Gateway started (PID: $GATEWAY_PID)"

# Health check - ensure gateway is responsive before load test
echo "  Checking gateway health..."
for i in {1..5}; do
    if curl -s -m 2 http://localhost:8080/api/boards/board-1 > /dev/null 2>&1; then
        echo "  ✓ Gateway is responsive"
        break
    fi
    if [ $i -eq 5 ]; then
        echo "ERROR: Gateway not responding after 5 attempts"
        exit 1
    fi
    sleep 1
done

echo ""
echo "Step 2: Running load test with $NUM_CLIENTS concurrent clients..."
echo ""

# Function to get millisecond timestamp (works on macOS and Linux)
get_ms_timestamp() {
    if [[ "$OSTYPE" == "darwin"* ]]; then
        # macOS - use perl
        perl -MTime::HiRes=time -e 'printf "%.0f\n", time * 1000'
    else
        # Linux
        date +%s%3N
    fi
}
export -f get_ms_timestamp >/dev/null 2>&1

# Function for a single client's work
client_work() {
    local client_id=$1
    local results_file="$RESULTS_DIR/client_${client_id}.txt"
    local success_count=0
    local fail_count=0
    local total_time=0
    
    for i in $(seq 1 $TASKS_PER_CLIENT); do
        local start_time=$(get_ms_timestamp)
        
        local response=$(curl -s -m 10 -w "\n%{http_code}" -X POST http://localhost:8080/api/tasks \
            -H "Content-Type: application/json" \
            -d "{\"title\":\"Client${client_id}-Task${i}\",\"description\":\"Load test task\",\"column\":0,\"created_by\":\"client${client_id}\",\"board_id\":\"board-1\"}" \
            2>/dev/null)
        
        local end_time=$(get_ms_timestamp)
        local latency=$((end_time - start_time))
        total_time=$((total_time + latency))
        
        local http_code=$(echo "$response" | tail -n1)
        local body=$(echo "$response" | sed '$d')
        
        if [ "$http_code" = "201" ] && echo "$body" | grep -q "task_id"; then
            success_count=$((success_count + 1))
            echo "OK $latency" >> "$results_file"
        else
            fail_count=$((fail_count + 1))
            echo "FAIL $latency" >> "$results_file"
        fi
        
        # Small random delay (0-500ms) to simulate human speed
        sleep 0.$(( RANDOM % 5 ))
    done
    
    echo "$client_id $success_count $fail_count $total_time" >> "$RESULTS_DIR/summary.txt"
}
export -f client_work >/dev/null 2>&1

# Record start time
START_TIME=$(date +%s)

# Launch all clients in parallel
echo "  Launching $NUM_CLIENTS clients..."
CLIENT_PIDS=""
for client_id in $(seq 1 $NUM_CLIENTS); do
    client_work $client_id &
    CLIENT_PIDS="$CLIENT_PIDS $!"
done

# Wait for all client processes to complete (not server processes)
echo "  Waiting for all clients to complete..."
for pid in $CLIENT_PIDS; do
    wait $pid 2>/dev/null
done

END_TIME=$(date +%s)
TOTAL_DURATION=$((END_TIME - START_TIME))

echo ""
echo "Step 3: Analyzing results..."
echo ""

# Count results
TOTAL_SUCCESS=0
TOTAL_FAIL=0
ALL_LATENCIES=""

while read -r line; do
    client_id=$(echo "$line" | awk '{print $1}')
    success=$(echo "$line" | awk '{print $2}')
    fail=$(echo "$line" | awk '{print $3}')
    TOTAL_SUCCESS=$((TOTAL_SUCCESS + success))
    TOTAL_FAIL=$((TOTAL_FAIL + fail))
done < "$RESULTS_DIR/summary.txt"

# Collect all latencies
for f in "$RESULTS_DIR"/client_*.txt; do
    if [ -f "$f" ]; then
        while read -r status latency; do
            if [ "$status" = "OK" ]; then
                ALL_LATENCIES="$ALL_LATENCIES $latency"
            fi
        done < "$f"
    fi
done

# Calculate latency statistics
if [ -n "$ALL_LATENCIES" ]; then
    # Sort latencies and calculate percentiles
    SORTED=$(echo $ALL_LATENCIES | tr ' ' '\n' | sort -n)
    COUNT=$(echo "$SORTED" | wc -l | tr -d ' ')
    
    MIN=$(echo "$SORTED" | head -1)
    MAX=$(echo "$SORTED" | tail -1)
    
    # Calculate average
    SUM=0
    for lat in $ALL_LATENCIES; do
        SUM=$((SUM + lat))
    done
    AVG=$((SUM / COUNT))
    
    # 90th percentile index
    P90_IDX=$(( (COUNT * 90) / 100 ))
    P90=$(echo "$SORTED" | sed -n "${P90_IDX}p")
    
    # 99th percentile index
    P99_IDX=$(( (COUNT * 99) / 100 ))
    P99=$(echo "$SORTED" | sed -n "${P99_IDX}p")
fi

# Verify task count in backend
echo "  Verifying task count in backend..."
BOARD_RESPONSE=$(curl -s http://localhost:8080/api/boards/board-1 2>/dev/null)
ACTUAL_COUNT=$(echo "$BOARD_RESPONSE" | grep -o '"task_id"' | wc -l | tr -d ' ')

echo ""
echo "=========================================="
echo "LOAD TEST RESULTS"
echo "=========================================="
echo ""
echo "Configuration:"
echo "  Clients: $NUM_CLIENTS"
echo "  Tasks per client: $TASKS_PER_CLIENT"
echo "  Expected total: $TOTAL_EXPECTED"
echo ""
echo "Results:"
echo "  Successful operations: $TOTAL_SUCCESS"
echo "  Failed operations: $TOTAL_FAIL"
echo "  Tasks in backend: $ACTUAL_COUNT"
echo "  Total duration: ${TOTAL_DURATION}s"
echo ""
echo "Latency Statistics (ms):"
echo "  Min: ${MIN:-N/A}"
echo "  Max: ${MAX:-N/A}"
echo "  Average: ${AVG:-N/A}"
echo "  90th percentile: ${P90:-N/A}"
echo "  99th percentile: ${P99:-N/A}"
echo ""

# Check for deadlocks (master should still be responsive)
echo "Checking for deadlocks..."
HEALTH_CHECK=$(curl -s -m 5 http://localhost:8080/api/boards/board-1 2>/dev/null)
if echo "$HEALTH_CHECK" | grep -q "board_id"; then
    echo "  ✓ System still responsive (no deadlock)"
else
    echo "  ✗ System unresponsive (possible deadlock)"
fi

echo ""
echo "=========================================="

# Determine pass/fail
if [ "$TOTAL_SUCCESS" -eq "$TOTAL_EXPECTED" ] && [ "$ACTUAL_COUNT" -eq "$TOTAL_EXPECTED" ]; then
    echo "TEST PASSED: All $TOTAL_EXPECTED tasks created successfully"
    echo "=========================================="
    exit 0
else
    echo "TEST FAILED:"
    if [ "$TOTAL_SUCCESS" -ne "$TOTAL_EXPECTED" ]; then
        echo "  - Expected $TOTAL_EXPECTED successful ops, got $TOTAL_SUCCESS"
    fi
    if [ "$ACTUAL_COUNT" -ne "$TOTAL_EXPECTED" ]; then
        echo "  - Expected $TOTAL_EXPECTED tasks in backend, got $ACTUAL_COUNT"
    fi
    echo "=========================================="
    echo ""
    echo "=== Master Log (last 20 lines) ==="
    tail -20 /tmp/master_load.log
    exit 1
fi
