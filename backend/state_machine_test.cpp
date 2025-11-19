#include <iostream>
#include <cassert>
#include <stdexcept>
#include "state_machine.h"
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

/* ============ LogEntry Tests ============ */

TEST(test_log_entry_creation)
{
    VectorClock vc(1);
    vc.increment();

    LogEntry entry(0, OpType::CREATE_TASK, vc, 0, "Test task", Column::TODO, 1);

    ASSERT_EQUAL(entry.get_entry_id(), 0);
    ASSERT_EQUAL(entry.get_op_type(), OpType::CREATE_TASK);
    ASSERT_EQUAL(entry.get_task_id(), 0);
    ASSERT_EQUAL(entry.get_description(), "Test task");
    ASSERT_EQUAL(entry.get_column(), Column::TODO);
    ASSERT_EQUAL(entry.get_client_id(), 1);
}

TEST(test_log_entry_update)
{
    VectorClock vc(1);
    LogEntry entry(1, OpType::UPDATE_TASK, vc, 5, "Updated description", Column::TODO, -1);

    ASSERT_EQUAL(entry.get_entry_id(), 1);
    ASSERT_EQUAL(entry.get_op_type(), OpType::UPDATE_TASK);
    ASSERT_EQUAL(entry.get_task_id(), 5);
    ASSERT_EQUAL(entry.get_description(), "Updated description");
}

TEST(test_log_entry_move)
{
    VectorClock vc(1);
    LogEntry entry(2, OpType::MOVE_TASK, vc, 3, "", Column::IN_PROGRESS, -1);

    ASSERT_EQUAL(entry.get_entry_id(), 2);
    ASSERT_EQUAL(entry.get_op_type(), OpType::MOVE_TASK);
    ASSERT_EQUAL(entry.get_task_id(), 3);
    ASSERT_EQUAL(entry.get_column(), Column::IN_PROGRESS);
}

TEST(test_log_entry_delete)
{
    VectorClock vc(1);
    LogEntry entry(3, OpType::DELETE_TASK, vc, 7, "", Column::TODO, -1);

    ASSERT_EQUAL(entry.get_entry_id(), 3);
    ASSERT_EQUAL(entry.get_op_type(), OpType::DELETE_TASK);
    ASSERT_EQUAL(entry.get_task_id(), 7);
}

/* ============ StateMachine Initialization Tests ============ */

TEST(test_state_machine_initialization)
{
    TaskManager tm;
    StateMachine sm(0, &tm);

    ASSERT_EQUAL(sm.get_server_id(), 0);
    ASSERT_EQUAL(sm.get_primary_id(), -1);
    ASSERT_EQUAL(sm.get_last_index(), -1);
    ASSERT_EQUAL(sm.get_committed_index(), -1);
}

TEST(test_state_machine_set_primary)
{
    TaskManager tm;
    StateMachine sm(1, &tm);

    sm.set_primary_id(0);
    ASSERT_EQUAL(sm.get_primary_id(), 0);
    ASSERT_FALSE(sm.is_primary());

    sm.set_primary_id(1);
    ASSERT_EQUAL(sm.get_primary_id(), 1);
    ASSERT_TRUE(sm.is_primary());
}

TEST(test_state_machine_is_primary)
{
    TaskManager tm;
    StateMachine sm(0, &tm);

    sm.set_primary_id(0);
    ASSERT_TRUE(sm.is_primary());

    sm.set_primary_id(1);
    ASSERT_FALSE(sm.is_primary());
}

/* ============ Log Management Tests ============ */

TEST(test_append_to_log)
{
    TaskManager tm;
    StateMachine sm(0, &tm);

    VectorClock vc(1);
    LogEntry entry(0, OpType::CREATE_TASK, vc, 0, "Task 1", Column::TODO, 1);

    sm.append_to_log(entry);

    ASSERT_EQUAL(sm.get_last_index(), 0);
}

TEST(test_append_multiple_to_log)
{
    TaskManager tm;
    StateMachine sm(0, &tm);

    VectorClock vc1(1);
    LogEntry entry1(0, OpType::CREATE_TASK, vc1, 0, "Task 1", Column::TODO, 1);

    VectorClock vc2(1);
    LogEntry entry2(1, OpType::CREATE_TASK, vc2, 1, "Task 2", Column::TODO, 1);

    sm.append_to_log(entry1);
    sm.append_to_log(entry2);

    ASSERT_EQUAL(sm.get_last_index(), 1);
}

TEST(test_get_log_entry)
{
    TaskManager tm;
    StateMachine sm(0, &tm);

    VectorClock vc(1);
    LogEntry entry(0, OpType::CREATE_TASK, vc, 0, "Test task", Column::TODO, 1);

    sm.append_to_log(entry);

    LogEntry retrieved = sm.get_log_entry(0);
    ASSERT_EQUAL(retrieved.get_entry_id(), 0);
    ASSERT_EQUAL(retrieved.get_description(), "Test task");
}

TEST(test_get_log_entry_invalid_index)
{
    TaskManager tm;
    StateMachine sm(0, &tm);

    bool exception_thrown = false;
    try
    {
        sm.get_log_entry(0);
    }
    catch (const std::out_of_range &)
    {
        exception_thrown = true;
    }

    ASSERT_TRUE(exception_thrown);
}

TEST(test_get_log)
{
    TaskManager tm;
    StateMachine sm(0, &tm);

    VectorClock vc1(1);
    LogEntry entry1(0, OpType::CREATE_TASK, vc1, 0, "Task 1", Column::TODO, 1);
    VectorClock vc2(1);
    LogEntry entry2(1, OpType::CREATE_TASK, vc2, 1, "Task 2", Column::TODO, 1);

    sm.append_to_log(entry1);
    sm.append_to_log(entry2);

    std::vector<LogEntry> log = sm.get_log();
    ASSERT_EQUAL(log.size(), 2);
    ASSERT_EQUAL(log[0].get_description(), "Task 1");
    ASSERT_EQUAL(log[1].get_description(), "Task 2");
}

TEST(test_get_log_after)
{
    TaskManager tm;
    StateMachine sm(0, &tm);

    VectorClock vc(1);
    for (int i = 0; i < 5; i++)
    {
        LogEntry entry(i, OpType::CREATE_TASK, vc, i, "Task " + std::to_string(i), Column::TODO, 1);
        sm.append_to_log(entry);
    }

    std::vector<LogEntry> after = sm.get_log_after(2);
    ASSERT_EQUAL(after.size(), 2); // Should get entries 3 and 4
    ASSERT_EQUAL(after[0].get_entry_id(), 3);
    ASSERT_EQUAL(after[1].get_entry_id(), 4);
}

TEST(test_get_log_after_empty)
{
    TaskManager tm;
    StateMachine sm(0, &tm);

    VectorClock vc(1);
    LogEntry entry(0, OpType::CREATE_TASK, vc, 0, "Task", Column::TODO, 1);
    sm.append_to_log(entry);

    std::vector<LogEntry> after = sm.get_log_after(0);
    ASSERT_EQUAL(after.size(), 0);
}

/* ============ Apply Entry Tests ============ */

TEST(test_apply_create_entry)
{
    TaskManager tm;
    StateMachine sm(0, &tm);

    VectorClock vc(1);
    LogEntry entry(0, OpType::CREATE_TASK, vc, 0, "New task", Column::TODO, 1);

    sm.append_to_log(entry);
    sm.apply_entry(0);

    ASSERT_EQUAL(tm.get_task_count(), 1);
    Task task = tm.get_task(0);
    ASSERT_EQUAL(task.get_description(), "New task");
}

TEST(test_apply_update_entry)
{
    TaskManager tm;
    StateMachine sm(0, &tm);

    // First create a task
    tm.create_task("Original", 1);

    // Now update via log
    VectorClock vc(1);
    LogEntry entry(0, OpType::UPDATE_TASK, vc, 0, "Updated", Column::TODO, -1);

    sm.append_to_log(entry);
    sm.apply_entry(0);

    Task task = tm.get_task(0);
    ASSERT_EQUAL(task.get_description(), "Updated");
}

TEST(test_apply_move_entry)
{
    TaskManager tm;
    StateMachine sm(0, &tm);

    // Create a task
    tm.create_task("Task", 1);

    // Move via log
    VectorClock vc(1);
    LogEntry entry(0, OpType::MOVE_TASK, vc, 0, "", Column::IN_PROGRESS, -1);

    sm.append_to_log(entry);
    sm.apply_entry(0);

    Task task = tm.get_task(0);
    ASSERT_EQUAL(task.get_column(), Column::IN_PROGRESS);
}

TEST(test_apply_delete_entry)
{
    TaskManager tm;
    StateMachine sm(0, &tm);

    // Create a task
    tm.create_task("Task to delete", 1);
    ASSERT_EQUAL(tm.get_task_count(), 1);

    // Delete via log
    VectorClock vc(1);
    LogEntry entry(0, OpType::DELETE_TASK, vc, 0, "", Column::TODO, -1);

    sm.append_to_log(entry);
    sm.apply_entry(0);

    ASSERT_EQUAL(tm.get_task_count(), 0);
}

TEST(test_apply_entry_invalid_index)
{
    TaskManager tm;
    StateMachine sm(0, &tm);

    bool exception_thrown = false;
    try
    {
        sm.apply_entry(5);
    }
    catch (const std::out_of_range &)
    {
        exception_thrown = true;
    }

    ASSERT_TRUE(exception_thrown);
}

/* ============ Commit Tests ============ */

TEST(test_commit_up_to)
{
    TaskManager tm;
    StateMachine sm(0, &tm);

    VectorClock vc(1);
    LogEntry entry1(0, OpType::CREATE_TASK, vc, 0, "Task 1", Column::TODO, 1);
    LogEntry entry2(1, OpType::CREATE_TASK, vc, 1, "Task 2", Column::TODO, 1);

    sm.append_to_log(entry1);
    sm.append_to_log(entry2);

    sm.commit_up_to(1);

    ASSERT_EQUAL(sm.get_committed_index(), 1);
    ASSERT_EQUAL(tm.get_task_count(), 2);
}

TEST(test_commit_up_to_partial)
{
    TaskManager tm;
    StateMachine sm(0, &tm);

    VectorClock vc(1);
    for (int i = 0; i < 5; i++)
    {
        LogEntry entry(i, OpType::CREATE_TASK, vc, i, "Task " + std::to_string(i), Column::TODO, 1);
        sm.append_to_log(entry);
    }

    sm.commit_up_to(2); // Commit first 3 entries

    ASSERT_EQUAL(sm.get_committed_index(), 2);
    ASSERT_EQUAL(tm.get_task_count(), 3);
}

TEST(test_commit_already_committed)
{
    TaskManager tm;
    StateMachine sm(0, &tm);

    VectorClock vc(1);
    LogEntry entry(0, OpType::CREATE_TASK, vc, 0, "Task", Column::TODO, 1);

    sm.append_to_log(entry);
    sm.commit_up_to(0);

    ASSERT_EQUAL(tm.get_task_count(), 1);

    // Try to commit again - should be no-op
    sm.commit_up_to(0);
    ASSERT_EQUAL(tm.get_task_count(), 1);
}

TEST(test_commit_up_to_invalid_index)
{
    TaskManager tm;
    StateMachine sm(0, &tm);

    bool exception_thrown = false;
    try
    {
        sm.commit_up_to(5);
    }
    catch (const std::out_of_range &)
    {
        exception_thrown = true;
    }

    ASSERT_TRUE(exception_thrown);
}

/* ============ Replay Tests ============ */

TEST(test_replay_from)
{
    TaskManager tm;
    StateMachine sm(0, &tm);

    // Create some initial state
    tm.create_task("Task 0", 1);
    tm.create_task("Task 1", 1);

    // Add entries to log
    VectorClock vc(1);
    LogEntry entry2(2, OpType::CREATE_TASK, vc, 2, "Task 2", Column::TODO, 1);
    LogEntry entry3(3, OpType::CREATE_TASK, vc, 3, "Task 3", Column::TODO, 1);

    sm.append_to_log(entry2);
    sm.append_to_log(entry3);

    // Replay from index 0 (which is entry2)
    sm.replay_from(0);

    ASSERT_EQUAL(tm.get_task_count(), 4);      // 2 initial + 2 replayed
    ASSERT_EQUAL(sm.get_committed_index(), 1); // Both entries committed
}

TEST(test_replay_from_middle)
{
    TaskManager tm;
    StateMachine sm(0, &tm);

    VectorClock vc(1);
    for (int i = 0; i < 5; i++)
    {
        LogEntry entry(i, OpType::CREATE_TASK, vc, i, "Task " + std::to_string(i), Column::TODO, 1);
        sm.append_to_log(entry);
    }

    // Replay from index 3
    sm.replay_from(3);

    ASSERT_EQUAL(tm.get_task_count(), 2); // Only entries 3 and 4
    ASSERT_EQUAL(sm.get_committed_index(), 4);
}

/* ============ Integration Tests ============ */

TEST(test_primary_backup_workflow)
{
    // Primary
    TaskManager tm_primary;
    StateMachine sm_primary(0, &tm_primary);
    sm_primary.set_primary_id(0);

    ASSERT_TRUE(sm_primary.is_primary());

    // Primary receives operation
    VectorClock vc(1);
    LogEntry entry(0, OpType::CREATE_TASK, vc, 0, "Task from primary", Column::TODO, 1);

    sm_primary.append_to_log(entry);
    sm_primary.commit_up_to(0);

    ASSERT_EQUAL(tm_primary.get_task_count(), 1);

    // Backup
    TaskManager tm_backup;
    StateMachine sm_backup(1, &tm_backup);
    sm_backup.set_primary_id(0);

    ASSERT_FALSE(sm_backup.is_primary());

    // Backup receives log entry from primary
    sm_backup.append_to_log(entry);
    sm_backup.commit_up_to(0);

    ASSERT_EQUAL(tm_backup.get_task_count(), 1);

    // Verify both have same state
    Task task_primary = tm_primary.get_task(0);
    Task task_backup = tm_backup.get_task(0);

    ASSERT_EQUAL(task_primary.get_description(), task_backup.get_description());
}

TEST(test_backup_catchup)
{
    // Primary with operations
    TaskManager tm_primary;
    StateMachine sm_primary(0, &tm_primary);

    VectorClock vc(1);
    for (int i = 0; i < 5; i++)
    {
        LogEntry entry(i, OpType::CREATE_TASK, vc, i, "Task " + std::to_string(i), Column::TODO, 1);
        sm_primary.append_to_log(entry);
        sm_primary.commit_up_to(i);
    }

    // Backup joins late
    TaskManager tm_backup;
    StateMachine sm_backup(1, &tm_backup);

    // Backup gets all log entries from primary
    std::vector<LogEntry> log = sm_primary.get_log();

    for (const auto &entry : log)
    {
        sm_backup.append_to_log(entry);
    }

    sm_backup.commit_up_to(4);

    // Verify same state
    ASSERT_EQUAL(tm_primary.get_task_count(), tm_backup.get_task_count());
    ASSERT_EQUAL(sm_primary.get_last_index(), sm_backup.get_last_index());
}

TEST(test_complex_operations_sequence)
{
    TaskManager tm;
    StateMachine sm(0, &tm);

    VectorClock vc(1);

    // Create
    LogEntry create(0, OpType::CREATE_TASK, vc, 0, "Initial task", Column::TODO, 1);
    sm.append_to_log(create);
    sm.apply_entry(0);

    // Update
    LogEntry update(1, OpType::UPDATE_TASK, vc, 0, "Updated task", Column::TODO, -1);
    sm.append_to_log(update);
    sm.apply_entry(1);

    // Move
    LogEntry move(2, OpType::MOVE_TASK, vc, 0, "", Column::IN_PROGRESS, -1);
    sm.append_to_log(move);
    sm.apply_entry(2);

    // Verify final state
    Task task = tm.get_task(0);
    ASSERT_EQUAL(task.get_description(), "Updated task");
    ASSERT_EQUAL(task.get_column(), Column::IN_PROGRESS);

    // Move to done
    LogEntry done(3, OpType::MOVE_TASK, vc, 0, "", Column::DONE, -1);
    sm.append_to_log(done);
    sm.apply_entry(3);

    task = tm.get_task(0);
    ASSERT_EQUAL(task.get_column(), Column::DONE);
}

/* ============ Main Test Runner ============ */

int main()
{
    std::cout << "========================================" << std::endl;
    std::cout << "Running StateMachine Test Suite" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << std::endl;

    std::cout << "--- LogEntry Tests ---" << std::endl;
    RUN_TEST(test_log_entry_creation);
    RUN_TEST(test_log_entry_update);
    RUN_TEST(test_log_entry_move);
    RUN_TEST(test_log_entry_delete);
    std::cout << std::endl;

    std::cout << "--- StateMachine Initialization Tests ---" << std::endl;
    RUN_TEST(test_state_machine_initialization);
    RUN_TEST(test_state_machine_set_primary);
    RUN_TEST(test_state_machine_is_primary);
    std::cout << std::endl;

    std::cout << "--- Log Management Tests ---" << std::endl;
    RUN_TEST(test_append_to_log);
    RUN_TEST(test_append_multiple_to_log);
    RUN_TEST(test_get_log_entry);
    RUN_TEST(test_get_log_entry_invalid_index);
    RUN_TEST(test_get_log);
    RUN_TEST(test_get_log_after);
    RUN_TEST(test_get_log_after_empty);
    std::cout << std::endl;

    std::cout << "--- Apply Entry Tests ---" << std::endl;
    RUN_TEST(test_apply_create_entry);
    RUN_TEST(test_apply_update_entry);
    RUN_TEST(test_apply_move_entry);
    RUN_TEST(test_apply_delete_entry);
    RUN_TEST(test_apply_entry_invalid_index);
    std::cout << std::endl;

    std::cout << "--- Commit Tests ---" << std::endl;
    RUN_TEST(test_commit_up_to);
    RUN_TEST(test_commit_up_to_partial);
    RUN_TEST(test_commit_already_committed);
    RUN_TEST(test_commit_up_to_invalid_index);
    std::cout << std::endl;

    std::cout << "--- Replay Tests ---" << std::endl;
    RUN_TEST(test_replay_from);
    RUN_TEST(test_replay_from_middle);
    std::cout << std::endl;

    std::cout << "--- Integration Tests ---" << std::endl;
    RUN_TEST(test_primary_backup_workflow);
    RUN_TEST(test_backup_catchup);
    RUN_TEST(test_complex_operations_sequence);
    std::cout << std::endl;

    std::cout << "========================================" << std::endl;
    std::cout << "Test Results" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << "Tests Passed: " << tests_passed << std::endl;
    std::cout << "Tests Failed: " << tests_failed << std::endl;
    std::cout << "Total Tests:  " << (tests_passed + tests_failed) << std::endl;
    std::cout << std::endl;

    if (tests_failed == 0)
    {
        std::cout << "✓ All tests passed!" << std::endl;
        return 0;
    }
    else
    {
        std::cout << "✗ Some tests failed!" << std::endl;
        return 1;
    }
}