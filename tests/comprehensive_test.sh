#!/bin/bash

# =============================================================================
# Comprehensive End-to-End Test Suite
# Tests all system functionality including edge cases
# =============================================================================

# Note: Not using 'set -e' because we handle errors explicitly in each test

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Counters
TESTS_PASSED=0
TESTS_FAILED=0
TESTS_SKIPPED=0

# Get directories
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
BACKEND_DIR="$PROJECT_ROOT/backend"
GATEWAY_DIR="$PROJECT_ROOT/gateway"

# Log files
MASTER_LOG="/tmp/comprehensive_master.log"
BACKUP_LOG="/tmp/comprehensive_backup.log"
GATEWAY_LOG="/tmp/comprehensive_gateway.log"

# PIDs
MASTER_PID=""
BACKUP_PID=""
GATEWAY_PID=""

# =============================================================================
# Helper Functions
# =============================================================================

log_info() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

log_success() {
    echo -e "${GREEN}[PASS]${NC} $1"
    ((TESTS_PASSED++)) || true
}

log_fail() {
    echo -e "${RED}[FAIL]${NC} $1"
    ((TESTS_FAILED++)) || true
}

log_skip() {
    echo -e "${YELLOW}[SKIP]${NC} $1"
    ((TESTS_SKIPPED++)) || true
}

log_section() {
    echo ""
    echo -e "${YELLOW}============================================${NC}"
    echo -e "${YELLOW}$1${NC}"
    echo -e "${YELLOW}============================================${NC}"
}

cleanup() {
    log_info "Cleaning up processes..."
    [ -n "$MASTER_PID" ] && kill -9 $MASTER_PID 2>/dev/null || true
    [ -n "$BACKUP_PID" ] && kill -9 $BACKUP_PID 2>/dev/null || true
    [ -n "$GATEWAY_PID" ] && kill -9 $GATEWAY_PID 2>/dev/null || true
    pkill -9 -f "master 12345" 2>/dev/null || true
    pkill -9 -f "backup 12346" 2>/dev/null || true
    pkill -9 -f "node server.js" 2>/dev/null || true
    sleep 1
}

start_master() {
    log_info "Starting master on port 12345..."
    cd "$BACKEND_DIR"
    ./master 12345 0 127.0.0.1 12346 > "$MASTER_LOG" 2>&1 &
    MASTER_PID=$!
    sleep 2
    if ps -p $MASTER_PID > /dev/null 2>&1; then
        log_info "Master started (PID: $MASTER_PID)"
        return 0
    else
        log_fail "Master failed to start"
        return 1
    fi
}

start_backup() {
    log_info "Starting backup on port 12346..."
    cd "$BACKEND_DIR"
    ./backup 12346 1 127.0.0.1 12345 > "$BACKUP_LOG" 2>&1 &
    BACKUP_PID=$!
    sleep 2
    if ps -p $BACKUP_PID > /dev/null 2>&1; then
        log_info "Backup started (PID: $BACKUP_PID)"
        return 0
    else
        log_fail "Backup failed to start"
        return 1
    fi
}

start_gateway() {
    log_info "Starting gateway on port 8080..."
    cd "$GATEWAY_DIR"
    node server.js > "$GATEWAY_LOG" 2>&1 &
    GATEWAY_PID=$!
    sleep 3
    if ps -p $GATEWAY_PID > /dev/null 2>&1; then
        log_info "Gateway started (PID: $GATEWAY_PID)"
        return 0
    else
        log_fail "Gateway failed to start"
        return 1
    fi
}

start_full_stack() {
    cleanup
    start_backup
    sleep 1
    start_master
    sleep 2
    start_gateway
    sleep 2
}

# =============================================================================
# API Helper Functions
# =============================================================================

create_task() {
    local title="$1"
    local description="$2"
    local column="${3:-0}"
    local created_by="${4:-testuser}"
    
    curl -s --max-time 10 -X POST http://localhost:8080/api/tasks \
        -H "Content-Type: application/json" \
        -d "{\"title\":\"$title\",\"description\":\"$description\",\"column\":$column,\"created_by\":\"$created_by\",\"board_id\":\"board-1\"}"
}

update_task() {
    local task_id="$1"
    local title="$2"
    local description="$3"
    
    curl -s --max-time 10 -X PATCH "http://localhost:8080/api/tasks/$task_id" \
        -H "Content-Type: application/json" \
        -d "{\"title\":\"$title\",\"description\":\"$description\"}"
}

move_task() {
    local task_id="$1"
    local column="$2"
    
    curl -s --max-time 10 -X PATCH "http://localhost:8080/api/tasks/$task_id" \
        -H "Content-Type: application/json" \
        -d "{\"column\":$column}"
}

delete_task() {
    local task_id="$1"
    curl -s --max-time 10 -w "%{http_code}" -X DELETE "http://localhost:8080/api/tasks/$task_id"
}

get_board() {
    curl -s --max-time 10 http://localhost:8080/api/boards/board-1
}

get_task_count() {
    get_board | grep -o '"task_id"' | wc -l | tr -d ' '
}

# =============================================================================
# Test Functions
# =============================================================================

test_create_single_task() {
    log_info "Testing: Create single task"
    local response=$(create_task "Test Task" "Test Description" 0 "alice")
    
    if echo "$response" | grep -q '"task_id"'; then
        log_success "Create single task"
    else
        log_fail "Create single task - Response: $response"
    fi
}

test_create_task_all_columns() {
    log_info "Testing: Create tasks in all columns"
    
    local r1=$(create_task "TODO Task" "In TODO column" 0 "user1")
    local r2=$(create_task "Progress Task" "In progress column" 1 "user2")
    local r3=$(create_task "Done Task" "In done column" 2 "user3")
    
    local success=true
    echo "$r1" | grep -q '"task_id"' || success=false
    echo "$r2" | grep -q '"task_id"' || success=false
    echo "$r3" | grep -q '"task_id"' || success=false
    
    if $success; then
        log_success "Create tasks in all columns"
    else
        log_fail "Create tasks in all columns"
    fi
}

test_create_many_tasks() {
    log_info "Testing: Create 20 tasks rapidly"
    local success_count=0
    
    for i in $(seq 1 20); do
        local response=$(create_task "Bulk Task $i" "Description $i" $((i % 3)) "bulkuser")
        if echo "$response" | grep -q '"task_id"'; then
            ((success_count++))
        fi
    done
    
    if [ $success_count -eq 20 ]; then
        log_success "Create 20 tasks rapidly ($success_count/20)"
    else
        log_fail "Create 20 tasks rapidly ($success_count/20)"
    fi
}

test_create_task_empty_title() {
    log_info "Testing: Create task with empty title"
    local response=$(create_task "" "Description only" 0 "user")
    
    if echo "$response" | grep -q '"task_id"'; then
        log_success "Create task with empty title (accepted)"
    else
        log_fail "Create task with empty title"
    fi
}

test_create_task_long_content() {
    log_info "Testing: Create task with long content"
    local long_title=$(printf 'A%.0s' {1..200})
    local long_desc=$(printf 'B%.0s' {1..500})
    local response=$(create_task "$long_title" "$long_desc" 0 "longuser")
    
    if echo "$response" | grep -q '"task_id"'; then
        log_success "Create task with long content"
    else
        log_fail "Create task with long content"
    fi
}

test_create_task_special_chars() {
    log_info "Testing: Create task with special characters"
    # Use direct curl with single-quoted JSON to avoid bash escaping issues
    local response=$(curl -s -X POST http://localhost:8080/api/tasks \
        -H "Content-Type: application/json" \
        -d '{"title":"Task & Test <tag>","description":"Description with special chars!@#$%","column":0,"created_by":"special","board_id":"board-1"}')
    
    if echo "$response" | grep -q '"task_id"'; then
        log_success "Create task with special characters"
    else
        log_fail "Create task with special characters"
    fi
}

test_update_task_title() {
    log_info "Testing: Update task title"
    local create_response=$(create_task "Original Title" "Original Desc" 0 "user")
    local task_id=$(echo "$create_response" | grep -o '"task_id":[0-9]*' | grep -o '[0-9]*')
    
    if [ -z "$task_id" ]; then
        log_fail "Update task title - couldn't create task"
        return
    fi
    
    local update_response=$(update_task "$task_id" "Updated Title" "")
    
    if echo "$update_response" | grep -q '"task_id"'; then
        log_success "Update task title"
    else
        log_fail "Update task title"
    fi
}

test_update_task_description() {
    log_info "Testing: Update task description"
    local create_response=$(create_task "Title" "Original Desc" 0 "user")
    local task_id=$(echo "$create_response" | grep -o '"task_id":[0-9]*' | grep -o '[0-9]*')
    
    if [ -z "$task_id" ]; then
        log_fail "Update task description - couldn't create task"
        return
    fi
    
    local update_response=$(update_task "$task_id" "" "Updated Description")
    
    if echo "$update_response" | grep -q '"task_id"'; then
        log_success "Update task description"
    else
        log_fail "Update task description"
    fi
}

test_update_nonexistent_task() {
    log_info "Testing: Update non-existent task"
    local response=$(update_task 99999 "Title" "Desc")
    
    if echo "$response" | grep -qi "not found\|error"; then
        log_success "Update non-existent task (correctly rejected)"
    else
        log_fail "Update non-existent task - should have been rejected"
    fi
}

test_move_task_todo_to_progress() {
    log_info "Testing: Move task TODO -> IN_PROGRESS"
    local create_response=$(create_task "Move Test" "Desc" 0 "user")
    local task_id=$(echo "$create_response" | grep -o '"task_id":[0-9]*' | grep -o '[0-9]*')
    
    if [ -z "$task_id" ]; then
        log_fail "Move task - couldn't create task"
        return
    fi
    
    local move_response=$(move_task "$task_id" 1)
    
    if echo "$move_response" | grep -q '"task_id"'; then
        log_success "Move task TODO -> IN_PROGRESS"
    else
        log_fail "Move task TODO -> IN_PROGRESS"
    fi
}

test_move_task_progress_to_done() {
    log_info "Testing: Move task IN_PROGRESS -> DONE"
    local create_response=$(create_task "Move Test 2" "Desc" 1 "user")
    local task_id=$(echo "$create_response" | grep -o '"task_id":[0-9]*' | grep -o '[0-9]*')
    
    if [ -z "$task_id" ]; then
        log_fail "Move task - couldn't create task"
        return
    fi
    
    local move_response=$(move_task "$task_id" 2)
    
    if echo "$move_response" | grep -q '"task_id"'; then
        log_success "Move task IN_PROGRESS -> DONE"
    else
        log_fail "Move task IN_PROGRESS -> DONE"
    fi
}

test_move_task_done_to_todo() {
    log_info "Testing: Move task DONE -> TODO"
    local create_response=$(create_task "Move Test 3" "Desc" 2 "user")
    local task_id=$(echo "$create_response" | grep -o '"task_id":[0-9]*' | grep -o '[0-9]*')
    
    if [ -z "$task_id" ]; then
        log_fail "Move task - couldn't create task"
        return
    fi
    
    local move_response=$(move_task "$task_id" 0)
    
    if echo "$move_response" | grep -q '"task_id"'; then
        log_success "Move task DONE -> TODO"
    else
        log_fail "Move task DONE -> TODO"
    fi
}

test_delete_task() {
    log_info "Testing: Delete task"
    local create_response=$(create_task "Delete Me" "To be deleted" 0 "user")
    local task_id=$(echo "$create_response" | grep -o '"task_id":[0-9]*' | grep -o '[0-9]*')
    
    if [ -z "$task_id" ]; then
        log_fail "Delete task - couldn't create task"
        return
    fi
    
    local delete_response=$(delete_task "$task_id")
    
    if [ "$delete_response" = "204" ] || [ "$delete_response" = "200" ]; then
        log_success "Delete task"
    else
        log_fail "Delete task - Response: $delete_response"
    fi
}

test_delete_nonexistent_task() {
    log_info "Testing: Delete non-existent task"
    local response=$(delete_task 99999)
    
    # Should return 404 or similar
    if [ "$response" = "404" ] || echo "$response" | grep -qi "not found"; then
        log_success "Delete non-existent task (correctly rejected)"
    else
        log_fail "Delete non-existent task - got: $response"
    fi
}

test_get_board() {
    log_info "Testing: Get board"
    local response=$(get_board)
    
    if echo "$response" | grep -q '"board_id"'; then
        log_success "Get board"
    else
        log_fail "Get board"
    fi
}

test_get_board_after_operations() {
    log_info "Testing: Get board reflects operations"
    
    # Create a task
    local create_response=$(create_task "Board Test Task" "Desc" 0 "user")
    local task_id=$(echo "$create_response" | grep -o '"task_id":[0-9]*' | grep -o '[0-9]*')
    
    # Get board should include this task
    local board=$(get_board)
    
    if echo "$board" | grep -q "Board Test Task"; then
        log_success "Get board reflects operations"
    else
        log_fail "Get board reflects operations"
    fi
}

test_rapid_operations() {
    log_info "Testing: Rapid sequential operations"
    local task_ids=()
    
    # Create 5 tasks rapidly
    for i in 1 2 3 4 5; do
        local response=$(create_task "Rapid $i" "Desc $i" 0 "rapiduser")
        local tid=$(echo "$response" | grep -o '"task_id":[0-9]*' | grep -o '[0-9]*')
        if [ -n "$tid" ]; then
            task_ids+=("$tid")
        fi
    done
    
    # Update them all
    for tid in "${task_ids[@]}"; do
        update_task "$tid" "Updated Rapid" "" > /dev/null
    done
    
    # Move them all
    for tid in "${task_ids[@]}"; do
        move_task "$tid" 1 > /dev/null
    done
    
    # Delete them all
    local delete_count=0
    for tid in "${task_ids[@]}"; do
        local result=$(delete_task "$tid")
        if [ "$result" = "204" ] || [ "$result" = "200" ]; then
            ((delete_count++))
        fi
    done
    
    if [ $delete_count -eq ${#task_ids[@]} ]; then
        log_success "Rapid sequential operations"
    else
        log_fail "Rapid sequential operations ($delete_count/${#task_ids[@]} deleted)"
    fi
}

# =============================================================================
# Replication Tests
# =============================================================================

test_replication_create() {
    log_info "Testing: Replication of CREATE operations"
    
    # Create task via master
    create_task "Replicated Task" "Should appear on backup" 0 "repuser" > /dev/null
    
    sleep 2  # Wait for replication
    
    # Check backup log for replication
    if grep -q "Replicated CREATE_TASK" "$BACKUP_LOG"; then
        log_success "Replication of CREATE operations"
    else
        log_fail "Replication of CREATE operations"
    fi
}

test_replication_update() {
    log_info "Testing: Replication of UPDATE operations"
    
    # Create and update task
    local response=$(create_task "Update Replication" "Original" 0 "user")
    local task_id=$(echo "$response" | grep -o '"task_id":[0-9]*' | grep -o '[0-9]*')
    
    if [ -n "$task_id" ]; then
        update_task "$task_id" "Changed" "" > /dev/null
        sleep 2
        
        if grep -q "Replicated UPDATE_TASK" "$BACKUP_LOG"; then
            log_success "Replication of UPDATE operations"
        else
            log_fail "Replication of UPDATE operations"
        fi
    else
        log_fail "Replication of UPDATE operations - couldn't create task"
    fi
}

test_replication_move() {
    log_info "Testing: Replication of MOVE operations"
    
    local response=$(create_task "Move Replication" "Desc" 0 "user")
    local task_id=$(echo "$response" | grep -o '"task_id":[0-9]*' | grep -o '[0-9]*')
    
    if [ -n "$task_id" ]; then
        move_task "$task_id" 2 > /dev/null
        sleep 2
        
        if grep -q "Replicated MOVE_TASK" "$BACKUP_LOG"; then
            log_success "Replication of MOVE operations"
        else
            log_fail "Replication of MOVE operations"
        fi
    else
        log_fail "Replication of MOVE operations - couldn't create task"
    fi
}

test_replication_delete() {
    log_info "Testing: Replication of DELETE operations"
    
    local response=$(create_task "Delete Replication" "Desc" 0 "user")
    local task_id=$(echo "$response" | grep -o '"task_id":[0-9]*' | grep -o '[0-9]*')
    
    if [ -n "$task_id" ]; then
        delete_task "$task_id" > /dev/null
        sleep 2
        
        if grep -q "Replicated DELETE_TASK" "$BACKUP_LOG"; then
            log_success "Replication of DELETE operations"
        else
            log_fail "Replication of DELETE operations"
        fi
    else
        log_fail "Replication of DELETE operations - couldn't create task"
    fi
}

# =============================================================================
# Failover Tests
# =============================================================================

test_failover_basic() {
    log_info "Testing: Basic failover to backup"
    
    # Create some tasks before failover
    create_task "Pre-failover 1" "Desc" 0 "user" > /dev/null
    create_task "Pre-failover 2" "Desc" 1 "user" > /dev/null
    sleep 2  # Let replication complete
    
    # Kill master
    log_info "Killing master..."
    kill -9 $MASTER_PID 2>/dev/null
    MASTER_PID=""
    
    sleep 6  # Wait for backup promotion (gateway has 5s timeout)
    
    # Try creating task via backup (through gateway failover)
    local response=$(create_task "Post-failover" "Created after failover" 0 "failoveruser")
    
    if echo "$response" | grep -q '"task_id"'; then
        log_success "Basic failover to backup"
    else
        log_fail "Basic failover to backup"
    fi
}

test_failover_data_preserved() {
    log_info "Testing: Data preserved after failover"
    
    # Check if pre-failover tasks exist
    local board=$(get_board)
    
    if echo "$board" | grep -q "Pre-failover"; then
        log_success "Data preserved after failover"
    else
        log_fail "Data preserved after failover"
    fi
}

# =============================================================================
# Heartbeat Tests
# =============================================================================

test_heartbeat_running() {
    log_info "Testing: Heartbeat mechanism active"
    
    sleep 6  # Wait for at least one heartbeat cycle
    
    if grep -q "\[HEARTBEAT\]" "$MASTER_LOG"; then
        log_success "Heartbeat mechanism active"
    else
        log_fail "Heartbeat mechanism active"
    fi
}

# =============================================================================
# Edge Case Tests
# =============================================================================

test_concurrent_creates() {
    log_info "Testing: Concurrent task creation (simulated)"
    
    # Create 5 tasks rapidly (sequential but fast - true concurrency needs multiple terminals)
    local success_count=0
    for i in 1 2 3 4 5; do
        local response=$(create_task "Concurrent $i" "Desc" 0 "user$i")
        if echo "$response" | grep -q '"task_id"'; then
            ((success_count++)) || true
        fi
    done
    
    if [ "$success_count" -ge 4 ]; then
        log_success "Concurrent task creation ($success_count/5 created)"
    else
        log_fail "Concurrent task creation ($success_count/5 created)"
    fi
}

test_update_same_task_twice() {
    log_info "Testing: Update same task twice rapidly"
    
    local response=$(create_task "Double Update" "Original" 0 "user")
    local task_id=$(echo "$response" | grep -o '"task_id":[0-9]*' | grep -o '[0-9]*')
    
    if [ -n "$task_id" ]; then
        # Update twice rapidly (sequential)
        update_task "$task_id" "First Update" "" > /dev/null 2>&1
        update_task "$task_id" "Second Update" "" > /dev/null 2>&1
        
        sleep 1
        local board=$(get_board)
        
        # Second update should have succeeded
        if echo "$board" | grep -q "Second Update"; then
            log_success "Update same task twice rapidly"
        else
            log_fail "Update same task twice rapidly"
        fi
    else
        log_fail "Update same task twice rapidly - couldn't create task"
    fi
}

# =============================================================================
# Main Test Execution
# =============================================================================

trap cleanup EXIT

echo ""
echo "=========================================================="
echo "   COMPREHENSIVE END-TO-END TEST SUITE"
echo "=========================================================="
echo ""

# Compile backend
log_section "Compiling Backend"
cd "$BACKEND_DIR"
make clean > /dev/null 2>&1
if make > /dev/null 2>&1; then
    log_success "Backend compilation"
else
    log_fail "Backend compilation"
    exit 1
fi

# Start full stack
log_section "Starting System Components"
start_full_stack

# Wait for system to stabilize and replication handshake to complete
sleep 5

# Run CRUD tests
log_section "CRUD Operation Tests"
test_create_single_task
test_create_task_all_columns
test_create_task_empty_title
test_create_task_long_content
test_create_task_special_chars
test_update_task_title
test_update_task_description
test_update_nonexistent_task
test_move_task_todo_to_progress
test_move_task_progress_to_done
test_move_task_done_to_todo
test_delete_task
test_delete_nonexistent_task
test_get_board
test_get_board_after_operations

# Run bulk tests
log_section "Bulk Operation Tests"
test_create_many_tasks
test_rapid_operations

# Run replication tests (same system - no restart needed)
log_section "Replication Tests"
sleep 2  # Brief pause to let any pending operations complete
test_replication_create
test_replication_update
test_replication_move
test_replication_delete
test_heartbeat_running

# Run edge case tests (same system - no restart needed)
log_section "Edge Case Tests"
test_concurrent_creates
test_update_same_task_twice

# Run failover tests (destructive - requires restart, run last)
log_section "Failover Tests"
cleanup
start_full_stack
sleep 5
test_failover_basic
test_failover_data_preserved

# Print summary
log_section "TEST SUMMARY"
echo ""
echo -e "Tests Passed:  ${GREEN}$TESTS_PASSED${NC}"
echo -e "Tests Failed:  ${RED}$TESTS_FAILED${NC}"
echo -e "Tests Skipped: ${YELLOW}$TESTS_SKIPPED${NC}"
echo -e "Total Tests:   $((TESTS_PASSED + TESTS_FAILED + TESTS_SKIPPED))"
echo ""

if [ $TESTS_FAILED -eq 0 ]; then
    echo -e "${GREEN}All tests passed!${NC}"
    exit 0
else
    echo -e "${RED}Some tests failed. Check logs:${NC}"
    echo "  Master: $MASTER_LOG"
    echo "  Backup: $BACKUP_LOG"
    echo "  Gateway: $GATEWAY_LOG"
    exit 1
fi
