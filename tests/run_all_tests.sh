#!/bin/bash

# Run All The Tests

echo "=========================================="
echo "Running All The Test Suites"
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
echo "Test Suite 6: Failover Test"
echo "----------------------------"
cleanup
./quick_failover.sh
TEST6_RESULT=$?

cleanup

echo ""
echo "=========================================="
echo "Test Summary"
echo "=========================================="
echo "Test Suite 1 (Unit Tests):        $([ $TEST1_RESULT -eq 0 ] && echo 'PASS' || echo 'FAIL')"
echo "Test Suite 2 (State Machine):     $([ $TEST2_RESULT -eq 0 ] && echo 'PASS' || echo 'FAIL')"
echo "Test Suite 3 (Replication):       $([ $TEST3_RESULT -eq 0 ] && echo 'PASS' || echo 'FAIL')"
echo "Test Suite 4 (Integration):       $([ $TEST4_RESULT -eq 0 ] && echo 'PASS' || echo 'FAIL')"
echo "Test Suite 5 (Conflicts):         $([ $TEST5_RESULT -eq 0 ] && echo 'PASS' || echo 'FAIL')"
echo "Test Suite 6 (Failover):          $([ $TEST6_RESULT -eq 0 ] && echo 'PASS' || echo 'FAIL')"
echo "=========================================="

TOTAL_FAILED=$((TEST1_RESULT + TEST2_RESULT + TEST3_RESULT + TEST4_RESULT + TEST5_RESULT + TEST6_RESULT))

if [ $TOTAL_FAILED -eq 0 ]; then
    echo "All tests passed"
    exit 0
else
    echo "Some tests failed"
    exit 1
fi
