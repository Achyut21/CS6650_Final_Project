#include <iostream>
#include <cassert>
#include "state_machine.h"
#include "task_manager.h"
#include "messages.h"

void test_append_to_log() {
    std::cout << "Testing append_to_log..." << std::flush;
    
    StateMachine sm;
    VectorClock vc(0);
    
    // LogEntry(entry_id, op_type, vc, task_id, title, description, created_by, column, client_id)
    LogEntry entry1(0, OpType::CREATE_TASK, vc, 0, "Task 1", "Desc 1", "user", Column::TODO, 1);
    sm.append_to_log(entry1);
    
    assert(sm.get_log_size() == 1);
    
    std::cout << " PASSED\n";
}

void test_get_log() {
    std::cout << "Testing get_log..." << std::flush;
    
    StateMachine sm;
    VectorClock vc(0);
    
    LogEntry entry1(0, OpType::CREATE_TASK, vc, 0, "Task 1", "Desc 1", "user", Column::TODO, 1);
    LogEntry entry2(1, OpType::CREATE_TASK, vc, 1, "Task 2", "Desc 2", "user", Column::TODO, 1);
    
    sm.append_to_log(entry1);
    sm.append_to_log(entry2);
    
    std::vector<LogEntry> log = sm.get_log();
    assert(log.size() == 2);
    
    std::cout << " PASSED\n";
}

void test_get_log_after() {
    std::cout << "Testing get_log_after..." << std::flush;
    
    StateMachine sm;
    VectorClock vc(0);
    
    LogEntry entry1(0, OpType::CREATE_TASK, vc, 0, "Task 1", "Desc 1", "user", Column::TODO, 1);
    LogEntry entry2(1, OpType::CREATE_TASK, vc, 1, "Task 2", "Desc 2", "user", Column::TODO, 1);
    LogEntry entry3(2, OpType::CREATE_TASK, vc, 2, "Task 3", "Desc 3", "user", Column::TODO, 1);
    
    sm.append_to_log(entry1);
    sm.append_to_log(entry2);
    sm.append_to_log(entry3);
    
    std::vector<LogEntry> after_0 = sm.get_log_after(0);
    assert(after_0.size() == 2);
    
    std::vector<LogEntry> after_1 = sm.get_log_after(1);
    assert(after_1.size() == 1);
    
    std::cout << " PASSED\n";
}

void test_replay_log_create() {
    std::cout << "Testing replay_log with CREATE..." << std::flush;
    
    StateMachine sm;
    TaskManager tm;
    VectorClock vc(0);
    
    LogEntry entry1(0, OpType::CREATE_TASK, vc, 0, "Task 1", "Desc 1", "user", Column::TODO, 1);
    LogEntry entry2(1, OpType::CREATE_TASK, vc, 1, "Task 2", "Desc 2", "user", Column::TODO, 1);
    
    sm.append_to_log(entry1);
    sm.append_to_log(entry2);
    
    std::vector<LogEntry> log = sm.get_log();
    sm.replay_log(tm, log);
    
    assert(tm.get_task_count() == 2);
    
    std::cout << " PASSED\n";
}

void test_replay_log_update() {
    std::cout << "Testing replay_log with UPDATE..." << std::flush;
    
    StateMachine sm;
    TaskManager tm;
    VectorClock vc(0);
    
    // Create task
    LogEntry entry1(0, OpType::CREATE_TASK, vc, 0, "Task", "Original", "user", Column::TODO, 1);
    sm.append_to_log(entry1);
    
    // Update task (title and created_by not used for UPDATE, but still need to provide)
    LogEntry entry2(1, OpType::UPDATE_TASK, vc, 0, "", "Updated", "", Column::TODO, 1);
    sm.append_to_log(entry2);
    
    std::vector<LogEntry> log = sm.get_log();
    sm.replay_log(tm, log);
    
    Task task = tm.get_task(0);
    assert(task.get_description() == "Updated");
    
    std::cout << " PASSED\n";
}

void test_replay_log_move() {
    std::cout << "Testing replay_log with MOVE..." << std::flush;
    
    StateMachine sm;
    TaskManager tm;
    VectorClock vc(0);
    
    // Create task
    LogEntry entry1(0, OpType::CREATE_TASK, vc, 0, "Task 1", "Desc", "user", Column::TODO, 1);
    sm.append_to_log(entry1);
    
    // Move task (title, description, created_by not used for MOVE)
    LogEntry entry2(1, OpType::MOVE_TASK, vc, 0, "", "", "", Column::IN_PROGRESS, 1);
    sm.append_to_log(entry2);
    
    std::vector<LogEntry> log = sm.get_log();
    sm.replay_log(tm, log);
    
    Task task = tm.get_task(0);
    assert(task.get_column() == Column::IN_PROGRESS);
    
    std::cout << " PASSED\n";
}

void test_replay_log_delete() {
    std::cout << "Testing replay_log with DELETE..." << std::flush;
    
    StateMachine sm;
    TaskManager tm;
    VectorClock vc(0);
    
    // Create two tasks
    LogEntry entry1(0, OpType::CREATE_TASK, vc, 0, "Task 1", "Desc 1", "user", Column::TODO, 1);
    LogEntry entry2(1, OpType::CREATE_TASK, vc, 1, "Task 2", "Desc 2", "user", Column::TODO, 1);
    sm.append_to_log(entry1);
    sm.append_to_log(entry2);
    
    // Delete first task (title, description, created_by not used for DELETE)
    LogEntry entry3(2, OpType::DELETE_TASK, vc, 0, "", "", "", Column::TODO, 1);
    sm.append_to_log(entry3);
    
    std::vector<LogEntry> log = sm.get_log();
    sm.replay_log(tm, log);
    
    assert(tm.get_task_count() == 1);
    
    std::cout << " PASSED\n";
}

void test_log_100_operations() {
    std::cout << "Testing logging 100 operations..." << std::flush;
    
    StateMachine sm;
    VectorClock vc(0);
    
    for (int i = 0; i < 100; i++) {
        LogEntry entry(i, OpType::CREATE_TASK, vc, i, "Task", "Desc", "user", Column::TODO, 1);
        sm.append_to_log(entry);
    }
    
    assert(sm.get_log_size() == 100);
    
    std::cout << " PASSED\n";
}

void test_replay_reconstructs_state() {
    std::cout << "Testing replay reconstructs state correctly..." << std::flush;
    
    StateMachine sm;
    TaskManager tm1;
    VectorClock vc(0);
    
    // Create 5 tasks in tm1
    for (int i = 0; i < 5; i++) {
        tm1.create_task("Task " + std::to_string(i), 1);
        LogEntry entry(i, OpType::CREATE_TASK, vc, i, "Task " + std::to_string(i), "Desc", "user", Column::TODO, 1);
        sm.append_to_log(entry);
    }
    
    // Create new TaskManager and replay log
    TaskManager tm2;
    std::vector<LogEntry> log = sm.get_log();
    sm.replay_log(tm2, log);
    
    // Both should have same number of tasks
    assert(tm1.get_task_count() == tm2.get_task_count());
    assert(tm2.get_task_count() == 5);
    
    std::cout << " PASSED\n";
}

int main() {
    std::cout << "==================================\n";
    std::cout << "Running State Machine Test Suite\n";
    std::cout << "==================================\n\n";
    
    test_append_to_log();
    test_get_log();
    test_get_log_after();
    test_replay_log_create();
    test_replay_log_update();
    test_replay_log_move();
    test_replay_log_delete();
    test_log_100_operations();
    test_replay_reconstructs_state();
    
    std::cout << "\n==================================\n";
    std::cout << "All State Machine Tests Passed!\n";
    std::cout << "==================================\n";
    
    return 0;
}
