#!/bin/bash

# Run All Test Suites

echo "=========================================="
echo "Running All Test Suites"
echo "=========================================="
echo ""

# Get script directory and project paths
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
BACKEND_DIR="$PROJECT_ROOT/backend"

# Cleanup between tests
cleanup() {
    pkill -9 master backup node 2>/dev/null
    sleep 2
}

# Make all scripts executable
chmod +x "$SCRIPT_DIR"/*.sh

echo "Test Suite 1: C++ Unit Tests"
echo "------------------------------"
cd "$BACKEND_DIR"
./run_tests
TEST1_RESULT=$?

echo ""
echo "Test Suite 2: State Machine Tests"
echo "----------------------------------"
./run_sm_tests
TEST2_RESULT=$?

echo ""
echo "Test Suite 3: Replication Test"
echo "-------------------------------"
cleanup
cd "$SCRIPT_DIR"
./test_replication.sh
TEST3_RESULT=$?

echo ""
echo "Test Suite 4: Integration Test"
echo "-------------------------------"
cleanup
./test_integration.sh
TEST4_RESULT=$?

echo ""
echo "Test Suite 5: Conflict Detection Test"
echo "--------------------------------------"
cleanup
./test_conflicts.sh
TEST5_RESULT=$?

echo ""
echo "Test Suite 6: Quick Failover Test"
echo "----------------------------------"
cleanup
./quick_failover.sh
TEST6_RESULT=$?

cleanup

echo ""
echo "=========================================="
echo "Test Summary"
echo "=========================================="

PASS_COUNT=0
FAIL_COUNT=0

check_result() {
    if [ $1 -eq 0 ]; then
        echo "$2: PASS"
        ((PASS_COUNT++))
    else
        echo "$2: FAIL"
        ((FAIL_COUNT++))
    fi
}

check_result $TEST1_RESULT "Unit Tests"
check_result $TEST2_RESULT "State Machine"
check_result $TEST3_RESULT "Replication"
check_result $TEST4_RESULT "Integration"
check_result $TEST5_RESULT "Conflicts"
check_result $TEST6_RESULT "Failover"

echo "=========================================="
echo "Passed: $PASS_COUNT / 6"
echo "=========================================="

if [ $FAIL_COUNT -eq 0 ]; then
    echo "All tests passed!"
    exit 0
else
    echo "$FAIL_COUNT test(s) failed"
    exit 1
fi
