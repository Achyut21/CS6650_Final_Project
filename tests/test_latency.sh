#!/bin/bash

# Test Latency - Multi-client latency measurement
# Measures operation latency using curl timing for CREATE, UPDATE, MOVE, DELETE, GET operations

echo "=========================================="
echo "Latency Measurement Test"
echo "=========================================="
echo ""

# Get script directory and project paths
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
BACKEND_DIR="$PROJECT_ROOT/backend"
GATEWAY_DIR="$PROJECT_ROOT/gateway"

# Configuration
NUM_ITERATIONS=10
CONCURRENT_CLIENTS=3

# Arrays to store latency measurements (in milliseconds)
declare -a CREATE_LATENCIES
declare -a UPDATE_LATENCIES
declare -a MOVE_LATENCIES
declare -a DELETE_LATENCIES
declare -a GET_LATENCIES

# Cleanup function
cleanup() {
    echo ""
    echo "Cleaning up..."
    pkill -9 -f "master 12345" 2>/dev/null
    pkill -9 -f "backup 12346" 2>/dev/null
    pkill -9 -f "node server.js" 2>/dev/null
    rm -f /tmp/master_latency.log /tmp/backup_latency.log /tmp/gateway_latency.log 2>/dev/null
    rm -f /tmp/latency_*.txt 2>/dev/null
    sleep 1
}

trap cleanup EXIT

# Function to measure single operation latency (returns time in ms)
measure_latency() {
    local START=$(python3 -c 'import time; print(int(time.time() * 1000))')
    eval "$1" > /dev/null 2>&1
    local END=$(python3 -c 'import time; print(int(time.time() * 1000))')
    echo $((END - START))
}

# Function to calculate statistics (pass array elements as arguments)
calculate_stats() {
    local sum=0
    local min=999999
    local max=0
    local count=$#
    
    if [ $count -eq 0 ]; then
        echo "0 0 0 0"
        return
    fi
    
    for val in "$@"; do
        sum=$((sum + val))
        if [ $val -lt $min ]; then min=$val; fi
        if [ $val -gt $max ]; then max=$val; fi
    done
    
    local avg=$((sum / count))
    
    # Calculate 90th percentile (simple approximation)
    local sorted=($(printf '%s\n' "$@" | sort -n))
    local p90_idx=$(( (count * 90) / 100 ))
    if [ $p90_idx -ge $count ]; then p90_idx=$((count - 1)); fi
    local p90=${sorted[$p90_idx]}
    
    echo "$avg $min $max $p90"
}

# Start fresh
cleanup

echo "Starting system components..."
echo "--------------------------------------"

# Start Backup
cd "$BACKEND_DIR"
./backup 12346 1 127.0.0.1 12345 > /tmp/backup_latency.log 2>&1 &
BACKUP_PID=$!
sleep 1

# Start Master
./master 12345 0 127.0.0.1 12346 > /tmp/master_latency.log 2>&1 &
MASTER_PID=$!
sleep 2

# Start Gateway
cd "$GATEWAY_DIR"
node server.js > /tmp/gateway_latency.log 2>&1 &
GATEWAY_PID=$!
sleep 2

# Verify all components are running
if ! ps -p $MASTER_PID > /dev/null || ! ps -p $BACKUP_PID > /dev/null || ! ps -p $GATEWAY_PID > /dev/null; then
    echo "ERROR: Failed to start all components"
    exit 1
fi

echo "  All components started"
echo ""

echo "=========================================="
echo "Test 1: Single Client Latency"
echo "=========================================="
echo ""
echo "Running $NUM_ITERATIONS iterations per operation..."
echo ""

# Measure CREATE latency
echo -n "Measuring CREATE latency... "
for i in $(seq 1 $NUM_ITERATIONS); do
    LATENCY=$(measure_latency "curl -s -X POST http://localhost:8080/api/tasks -H 'Content-Type: application/json' -d '{\"title\":\"Latency Test $i\",\"description\":\"Test\",\"column\":0,\"created_by\":\"test\",\"board_id\":\"board-1\"}'")
    CREATE_LATENCIES+=($LATENCY)
done
echo "done"

# Get task IDs for update/move/delete tests
BOARD_RESPONSE=$(curl -s http://localhost:8080/api/boards/board-1)
TASK_IDS=($(echo "$BOARD_RESPONSE" | grep -o '"task_id":[0-9]*' | grep -o '[0-9]*'))

# Check if we have tasks to work with
if [ ${#TASK_IDS[@]} -eq 0 ]; then
    echo ""
    echo "ERROR: No tasks found for UPDATE/MOVE tests. CREATE may have failed."
    echo "Check gateway log: /tmp/gateway_latency.log"
    exit 1
fi

# Measure UPDATE latency
echo -n "Measuring UPDATE latency... "
for i in $(seq 1 $NUM_ITERATIONS); do
    TASK_ID=${TASK_IDS[$((i % ${#TASK_IDS[@]}))]}
    LATENCY=$(measure_latency "curl -s -X PATCH http://localhost:8080/api/tasks/$TASK_ID -H 'Content-Type: application/json' -d '{\"title\":\"Updated $i\",\"description\":\"Updated desc\"}'")
    UPDATE_LATENCIES+=($LATENCY)
done
echo "done"

# Measure MOVE latency
echo -n "Measuring MOVE latency... "
for i in $(seq 1 $NUM_ITERATIONS); do
    TASK_ID=${TASK_IDS[$((i % ${#TASK_IDS[@]}))]}
    COLUMN=$((i % 3))
    LATENCY=$(measure_latency "curl -s -X PATCH http://localhost:8080/api/tasks/$TASK_ID -H 'Content-Type: application/json' -d '{\"column\":$COLUMN}'")
    MOVE_LATENCIES+=($LATENCY)
done
echo "done"

# Measure GET (board) latency
echo -n "Measuring GET latency... "
for i in $(seq 1 $NUM_ITERATIONS); do
    LATENCY=$(measure_latency "curl -s http://localhost:8080/api/boards/board-1")
    GET_LATENCIES+=($LATENCY)
done
echo "done"

# Measure DELETE latency (create tasks first)
echo -n "Measuring DELETE latency... "
for i in $(seq 1 $NUM_ITERATIONS); do
    # Create a task to delete
    RESPONSE=$(curl -s -X POST http://localhost:8080/api/tasks \
      -H "Content-Type: application/json" \
      -d "{\"title\":\"Delete Test $i\",\"description\":\"To be deleted\",\"column\":0,\"created_by\":\"test\",\"board_id\":\"board-1\"}")
    TASK_ID=$(echo "$RESPONSE" | grep -o '"task_id":[0-9]*' | grep -o '[0-9]*')
    
    if [ -n "$TASK_ID" ]; then
        LATENCY=$(measure_latency "curl -s -X DELETE http://localhost:8080/api/tasks/$TASK_ID")
        DELETE_LATENCIES+=($LATENCY)
    fi
done
echo "done"

echo ""
echo "=========================================="
echo "Test 2: Concurrent Client Latency"
echo "=========================================="
echo ""
echo "Running $CONCURRENT_CLIENTS concurrent clients..."

# Start concurrent clients using explicit subshells (functions don't work reliably with &)
CONCURRENT_PIDS=()
for c in $(seq 1 $CONCURRENT_CLIENTS); do
    (
        OUTPUT_FILE="/tmp/latency_client_$c.txt"
        rm -f "$OUTPUT_FILE"
        for i in $(seq 1 5); do
            START=$(python3 -c 'import time; print(int(time.time() * 1000))')
            curl -s --max-time 10 -X POST http://localhost:8080/api/tasks \
              -H "Content-Type: application/json" \
              -d "{\"title\":\"Concurrent $c-$i\",\"description\":\"Test\",\"column\":0,\"created_by\":\"client$c\",\"board_id\":\"board-1\"}" > /dev/null 2>&1
            END=$(python3 -c 'import time; print(int(time.time() * 1000))')
            echo $((END - START)) >> "$OUTPUT_FILE"
        done
    ) &
    CONCURRENT_PIDS+=($!)
done

# Wait only for the concurrent client processes (not master/backup/gateway)
for pid in "${CONCURRENT_PIDS[@]}"; do
    wait $pid 2>/dev/null
done

# Collect concurrent results
declare -a CONCURRENT_LATENCIES
for c in $(seq 1 $CONCURRENT_CLIENTS); do
    if [ -f "/tmp/latency_client_$c.txt" ]; then
        while read -r latency; do
            CONCURRENT_LATENCIES+=($latency)
        done < "/tmp/latency_client_$c.txt"
    fi
done

echo "  Completed ${#CONCURRENT_LATENCIES[@]} concurrent operations"

echo ""
echo "=========================================="
echo "Latency Results (in milliseconds)"
echo "=========================================="
echo ""

# Print results
printf "%-15s %8s %8s %8s %8s\n" "Operation" "Avg" "Min" "Max" "P90"
printf "%-15s %8s %8s %8s %8s\n" "---------" "---" "---" "---" "---"

# CREATE stats
read AVG MIN MAX P90 <<< $(calculate_stats "${CREATE_LATENCIES[@]}")
printf "%-15s %8d %8d %8d %8d\n" "CREATE" $AVG $MIN $MAX $P90

# UPDATE stats
read AVG MIN MAX P90 <<< $(calculate_stats "${UPDATE_LATENCIES[@]}")
printf "%-15s %8d %8d %8d %8d\n" "UPDATE" $AVG $MIN $MAX $P90

# MOVE stats
read AVG MIN MAX P90 <<< $(calculate_stats "${MOVE_LATENCIES[@]}")
printf "%-15s %8d %8d %8d %8d\n" "MOVE" $AVG $MIN $MAX $P90

# DELETE stats
read AVG MIN MAX P90 <<< $(calculate_stats "${DELETE_LATENCIES[@]}")
printf "%-15s %8d %8d %8d %8d\n" "DELETE" $AVG $MIN $MAX $P90

# GET stats
read AVG MIN MAX P90 <<< $(calculate_stats "${GET_LATENCIES[@]}")
printf "%-15s %8d %8d %8d %8d\n" "GET_BOARD" $AVG $MIN $MAX $P90

echo ""
printf "%-15s %8s %8s %8s %8s\n" "---------" "---" "---" "---" "---"

# Concurrent stats
read AVG MIN MAX P90 <<< $(calculate_stats "${CONCURRENT_LATENCIES[@]}")
printf "%-15s %8d %8d %8d %8d\n" "CONCURRENT" $AVG $MIN $MAX $P90

echo ""
echo "=========================================="
echo "Performance Assessment"
echo "=========================================="
echo ""

# Calculate overall average
TOTAL_OPS=$((${#CREATE_LATENCIES[@]} + ${#UPDATE_LATENCIES[@]} + ${#MOVE_LATENCIES[@]} + ${#DELETE_LATENCIES[@]} + ${#GET_LATENCIES[@]}))
TOTAL_SUM=0
for lat in "${CREATE_LATENCIES[@]}" "${UPDATE_LATENCIES[@]}" "${MOVE_LATENCIES[@]}" "${DELETE_LATENCIES[@]}" "${GET_LATENCIES[@]}"; do
    TOTAL_SUM=$((TOTAL_SUM + lat))
done
OVERALL_AVG=$((TOTAL_SUM / TOTAL_OPS))

echo "Total operations measured: $TOTAL_OPS"
echo "Overall average latency: ${OVERALL_AVG}ms"
echo ""

# Performance thresholds
if [ $OVERALL_AVG -lt 100 ]; then
    echo "✓ EXCELLENT: Average latency under 100ms"
elif [ $OVERALL_AVG -lt 200 ]; then
    echo "✓ GOOD: Average latency under 200ms"
elif [ $OVERALL_AVG -lt 500 ]; then
    echo "⚠ ACCEPTABLE: Average latency under 500ms"
else
    echo "✗ POOR: Average latency over 500ms"
fi

# Check P90 for concurrent operations
read AVG MIN MAX P90 <<< $(calculate_stats "${CONCURRENT_LATENCIES[@]}")
if [ $P90 -lt 500 ]; then
    echo "✓ Concurrent P90 latency (${P90}ms) meets target (<500ms)"
else
    echo "⚠ Concurrent P90 latency (${P90}ms) exceeds target (500ms)"
fi

echo ""
echo "=========================================="
echo "Latency Test Complete"
echo "=========================================="
