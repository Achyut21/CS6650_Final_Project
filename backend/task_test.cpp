#include <iostream>
#include <cassert>
#include <stdexcept>
#include "task_manager.h"
#include "messages.h"

// Test counter
int tests_passed = 0;
int tests_failed = 0;

// Helper macros
#define TEST(name) void name()
#define ASSERT_TRUE(expr)                                                         \
    if (!(expr))                                                                  \
    {                                                                             \
        std::cerr << "FAILED: " << #expr << " at line " << __LINE__ << std::endl; \
        tests_failed++;                                                           \
        return;                                                                   \
    }
#define ASSERT_FALSE(expr) ASSERT_TRUE(!(expr))
#define ASSERT_EQUAL(a, b) ASSERT_TRUE((a) == (b))
#define RUN_TEST(test)                                      \
    std::cout << "Running " << #test << "..." << std::endl; \
    test();                                                 \
    tests_passed++;                                         \
    std::cout << "  PASSED" << std::endl;

/* ============ VectorClock Tests ============ */

TEST(test_vector_clock_initialization)
{
    VectorClock vc(0);
    ASSERT_EQUAL(vc.get(0), 0);
}

TEST(test_vector_clock_increment)
{
    VectorClock vc(0);
    vc.increment();
    ASSERT_EQUAL(vc.get(0), 1);
    vc.increment();
    ASSERT_EQUAL(vc.get(0), 2);
}

TEST(test_vector_clock_update)
{
    VectorClock vc1(0);
    VectorClock vc2(1);

    vc1.increment(); // vc1: {0:1}
    vc2.increment(); // vc2: {1:1}

    vc2.update(vc1); // vc2 should be {0:1, 1:2}

    ASSERT_EQUAL(vc2.get(0), 1);
    ASSERT_EQUAL(vc2.get(1), 2);
}

TEST(test_vector_clock_compare_less_than)
{
    VectorClock vc1(0);
    VectorClock vc2(1);

    vc1.increment(); // vc1: {0:1}
    vc2.update(vc1); // vc2: {0:1, 1:1}

    ASSERT_EQUAL(vc1.compare_to(vc2), -1); // vc1 < vc2
}

TEST(test_vector_clock_compare_greater_than)
{
    VectorClock vc1(0);
    VectorClock vc2(1);

    vc1.increment(); // vc1: {0:1}
    vc2.update(vc1); // vc2: {0:1, 1:1}

    ASSERT_EQUAL(vc2.compare_to(vc1), 1); // vc2 > vc1
}

TEST(test_vector_clock_compare_concurrent)
{
    VectorClock vc1(0);
    VectorClock vc2(1);

    vc1.increment(); // vc1: {0:1}
    vc2.increment(); // vc2: {1:1}

    ASSERT_EQUAL(vc1.compare_to(vc2), 0); // concurrent
}

TEST(test_vector_clock_get_nonexistent)
{
    VectorClock vc(0);
    ASSERT_EQUAL(vc.get(5), 0); // Non-existent process should return 0
}

/* ============ Task Tests ============ */

TEST(test_task_creation)
{
    Task task(1, "Test title", "Test task", "board-1", "user", Column::TODO, 100);

    ASSERT_EQUAL(task.get_task_id(), 1);
    ASSERT_EQUAL(task.get_title(), "Test title");
    ASSERT_EQUAL(task.get_description(), "Test task");
    ASSERT_EQUAL(task.get_column(), Column::TODO);
    ASSERT_EQUAL(task.get_client_id(), 100);
}

TEST(test_task_setters)
{
    Task task(1, "Title", "Original", "board-1", "user", Column::TODO, 100);

    task.set_description("Updated");
    ASSERT_EQUAL(task.get_description(), "Updated");

    task.set_column(Column::IN_PROGRESS);
    ASSERT_EQUAL(task.get_column(), Column::IN_PROGRESS);

    task.set_client_id(200);
    ASSERT_EQUAL(task.get_client_id(), 200);

    task.set_task_id(5);
    ASSERT_EQUAL(task.get_task_id(), 5);
}

TEST(test_task_vector_clock_access)
{
    Task task(1, "Title", "Test", "board-1", "user", Column::TODO, 100);

    // VectorClock should be initialized with client_id
    ASSERT_EQUAL(task.get_clock().get(100), 0);

    task.get_clock().increment();
    ASSERT_EQUAL(task.get_clock().get(100), 1);
}

/* ============ TaskManager Tests ============ */

TEST(test_task_manager_create_task)
{
    TaskManager tm;
    ASSERT_TRUE(tm.create_task("First task", 1));
    ASSERT_EQUAL(tm.get_task_count(), 1);
}

TEST(test_task_manager_create_multiple_tasks)
{
    TaskManager tm;
    tm.create_task("Task 1", 1);
    tm.create_task("Task 2", 1);
    tm.create_task("Task 3", 1);

    ASSERT_EQUAL(tm.get_task_count(), 3);
}

TEST(test_task_manager_get_task)
{
    TaskManager tm;
    tm.create_task("My task", 1);

    Task task = tm.get_task(0);
    ASSERT_EQUAL(task.get_task_id(), 0);
    ASSERT_EQUAL(task.get_description(), "My task");
    ASSERT_EQUAL(task.get_column(), Column::TODO);
}

TEST(test_task_manager_get_nonexistent_task)
{
    TaskManager tm;

    bool exception_thrown = false;
    try
    {
        tm.get_task(999);
    }
    catch (const std::runtime_error &)
    {
        exception_thrown = true;
    }

    ASSERT_TRUE(exception_thrown);
}

TEST(test_task_manager_update_task)
{
    TaskManager tm;
    tm.create_task("Original", 1);

    VectorClock vc(1);
    vc.increment();
    ASSERT_TRUE(tm.update_task(0, "Updated", vc));

    Task task = tm.get_task(0);
    ASSERT_EQUAL(task.get_description(), "Updated");
}

TEST(test_task_manager_update_nonexistent_task)
{
    TaskManager tm;
    VectorClock vc(1);
    ASSERT_FALSE(tm.update_task(999, "Updated", vc));
}

TEST(test_task_manager_move_task)
{
    TaskManager tm;
    tm.create_task("Task", 1);

    VectorClock vc(1);
    vc.increment();
    ASSERT_TRUE(tm.move_task(0, Column::IN_PROGRESS, vc));

    Task task = tm.get_task(0);
    ASSERT_EQUAL(task.get_column(), Column::IN_PROGRESS);
}

TEST(test_task_manager_move_task_same_column)
{
    TaskManager tm;
    tm.create_task("Task", 1);

    // Moving to same column should return true
    VectorClock vc(1);
    ASSERT_TRUE(tm.move_task(0, Column::TODO, vc));

    Task task = tm.get_task(0);
    ASSERT_EQUAL(task.get_column(), Column::TODO);
}

TEST(test_task_manager_move_nonexistent_task)
{
    TaskManager tm;
    VectorClock vc(1);
    ASSERT_FALSE(tm.move_task(999, Column::DONE, vc));
}

TEST(test_task_manager_delete_task)
{
    TaskManager tm;
    tm.create_task("Task to delete", 1);

    ASSERT_EQUAL(tm.get_task_count(), 1);
    ASSERT_TRUE(tm.delete_task(0));
    ASSERT_EQUAL(tm.get_task_count(), 0);
}

TEST(test_task_manager_delete_nonexistent_task)
{
    TaskManager tm;
    ASSERT_FALSE(tm.delete_task(999));
}

TEST(test_task_manager_delete_and_recreate)
{
    TaskManager tm;
    tm.create_task("First", 1);
    tm.delete_task(0);
    tm.create_task("Second", 1);

    // Should be able to get the new task
    Task task = tm.get_task(1);
    ASSERT_EQUAL(task.get_description(), "Second");
}

TEST(test_task_manager_workflow)
{
    TaskManager tm;
    VectorClock vc(1);

    // Create tasks
    tm.create_task("Design UI", 1);
    tm.create_task("Implement backend", 1);
    tm.create_task("Write tests", 1);

    ASSERT_EQUAL(tm.get_task_count(), 3);

    // Move first task to IN_PROGRESS
    vc.increment();
    tm.move_task(0, Column::IN_PROGRESS, vc);
    ASSERT_EQUAL(tm.get_task(0).get_column(), Column::IN_PROGRESS);

    // Update description of second task
    vc.increment();
    tm.update_task(1, "Implement distributed backend", vc);
    ASSERT_EQUAL(tm.get_task(1).get_description(), "Implement distributed backend");

    // Complete first task
    vc.increment();
    tm.move_task(0, Column::DONE, vc);
    ASSERT_EQUAL(tm.get_task(0).get_column(), Column::DONE);

    // Delete third task
    tm.delete_task(2);
    ASSERT_EQUAL(tm.get_task_count(), 2);
}

/* ============ Integration Tests ============ */

TEST(test_task_vector_clock_increments)
{
    TaskManager tm;
    tm.create_task("Task 1", 1);

    Task task = tm.get_task(0);

    // Task's VectorClock should be initialized with client_id (1 in this case)
    ASSERT_EQUAL(task.get_clock().get(1), 0);

    task.get_clock().increment();
    ASSERT_EQUAL(task.get_clock().get(1), 1);
}

/* ============ Main Test Runner ============ */

int main()
{
    std::cout << "==================================" << std::endl;
    std::cout << "Running Task Manager Test Suite" << std::endl;
    std::cout << "==================================" << std::endl;
    std::cout << std::endl;

    std::cout << "--- VectorClock Tests ---" << std::endl;
    RUN_TEST(test_vector_clock_initialization);
    RUN_TEST(test_vector_clock_increment);
    RUN_TEST(test_vector_clock_update);
    RUN_TEST(test_vector_clock_compare_less_than);
    RUN_TEST(test_vector_clock_compare_greater_than);
    RUN_TEST(test_vector_clock_compare_concurrent);
    RUN_TEST(test_vector_clock_get_nonexistent);
    std::cout << std::endl;

    std::cout << "--- Task Tests ---" << std::endl;
    RUN_TEST(test_task_creation);
    RUN_TEST(test_task_setters);
    RUN_TEST(test_task_vector_clock_access);
    std::cout << std::endl;

    std::cout << "--- TaskManager Tests ---" << std::endl;
    RUN_TEST(test_task_manager_create_task);
    RUN_TEST(test_task_manager_create_multiple_tasks);
    RUN_TEST(test_task_manager_get_task);
    RUN_TEST(test_task_manager_get_nonexistent_task);
    RUN_TEST(test_task_manager_update_task);
    RUN_TEST(test_task_manager_update_nonexistent_task);
    RUN_TEST(test_task_manager_move_task);
    RUN_TEST(test_task_manager_move_task_same_column);
    RUN_TEST(test_task_manager_move_nonexistent_task);
    RUN_TEST(test_task_manager_delete_task);
    RUN_TEST(test_task_manager_delete_nonexistent_task);
    RUN_TEST(test_task_manager_delete_and_recreate);
    RUN_TEST(test_task_manager_workflow);
    std::cout << std::endl;

    std::cout << "--- Integration Tests ---" << std::endl;
    RUN_TEST(test_task_vector_clock_increments);
    std::cout << std::endl;

    std::cout << "==================================" << std::endl;
    std::cout << "Test Results" << std::endl;
    std::cout << "==================================" << std::endl;
    std::cout << "Tests Passed: " << tests_passed << std::endl;
    std::cout << "Tests Failed: " << tests_failed << std::endl;
    std::cout << "Total Tests:  " << (tests_passed + tests_failed) << std::endl;
    std::cout << std::endl;

    if (tests_failed == 0)
    {
        std::cout << "All tests passed!" << std::endl;
        return 0;
    }
    else
    {
        std::cout << "Some tests failed!" << std::endl;
        return 1;
    }
}