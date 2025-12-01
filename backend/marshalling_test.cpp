/**
 * Comprehensive Marshalling/Unmarshalling Tests
 * Tests binary serialization for Task and LogEntry classes
 */

#include <iostream>
#include <cassert>
#include <cstring>
#include <vector>
#include "messages.h"

int tests_passed = 0;
int tests_failed = 0;

#define TEST(name) void name()
#define RUN_TEST(test) \
    std::cout << "Running " << #test << "..." << std::flush; \
    try { test(); tests_passed++; std::cout << " PASSED\n"; } \
    catch (const std::exception& e) { tests_failed++; std::cout << " FAILED: " << e.what() << "\n"; } \
    catch (...) { tests_failed++; std::cout << " FAILED: Unknown error\n"; }

#define ASSERT_EQ(a, b) if ((a) != (b)) { throw std::runtime_error("Assertion failed: " #a " != " #b); }
#define ASSERT_TRUE(x) if (!(x)) { throw std::runtime_error("Assertion failed: " #x); }

/* ============ Task Marshalling Tests ============ */

TEST(test_task_marshal_unmarshal_basic) {
    Task original(1, "Test Title", "Test Description", "board-1", "alice", Column::TODO, 100);
    
    int size = original.Size();
    char* buffer = new char[size];
    original.Marshal(buffer);
    
    Task restored;
    restored.Unmarshal(buffer);
    delete[] buffer;
    
    ASSERT_EQ(restored.get_task_id(), 1);
    ASSERT_EQ(restored.get_title(), "Test Title");
    ASSERT_EQ(restored.get_description(), "Test Description");
    ASSERT_EQ(restored.get_board_id(), "board-1");
    ASSERT_EQ(restored.get_created_by(), "alice");
    ASSERT_EQ(restored.get_column(), Column::TODO);
    ASSERT_EQ(restored.get_client_id(), 100);
}

TEST(test_task_marshal_unmarshal_in_progress) {
    Task original(2, "Title", "Desc", "board-2", "bob", Column::IN_PROGRESS, 200);
    
    int size = original.Size();
    char* buffer = new char[size];
    original.Marshal(buffer);
    
    Task restored;
    restored.Unmarshal(buffer);
    delete[] buffer;
    
    ASSERT_EQ(restored.get_column(), Column::IN_PROGRESS);
}

TEST(test_task_marshal_unmarshal_done) {
    Task original(3, "Title", "Desc", "board-3", "charlie", Column::DONE, 300);
    
    int size = original.Size();
    char* buffer = new char[size];
    original.Marshal(buffer);
    
    Task restored;
    restored.Unmarshal(buffer);
    delete[] buffer;
    
    ASSERT_EQ(restored.get_column(), Column::DONE);
}

TEST(test_task_marshal_empty_strings) {
    Task original(0, "", "", "", "", Column::TODO, 0);
    
    int size = original.Size();
    char* buffer = new char[size];
    original.Marshal(buffer);
    
    Task restored;
    restored.Unmarshal(buffer);
    delete[] buffer;
    
    ASSERT_EQ(restored.get_title(), "");
    ASSERT_EQ(restored.get_description(), "");
    ASSERT_EQ(restored.get_board_id(), "");
    ASSERT_EQ(restored.get_created_by(), "");
}

TEST(test_task_marshal_long_strings) {
    std::string long_title(200, 'A');
    std::string long_desc(500, 'B');
    std::string long_board(50, 'C');
    std::string long_user(100, 'D');
    
    Task original(999, long_title, long_desc, long_board, long_user, Column::DONE, 12345);
    
    int size = original.Size();
    char* buffer = new char[size];
    original.Marshal(buffer);
    
    Task restored;
    restored.Unmarshal(buffer);
    delete[] buffer;
    
    ASSERT_EQ(restored.get_title(), long_title);
    ASSERT_EQ(restored.get_description(), long_desc);
    ASSERT_EQ(restored.get_board_id(), long_board);
    ASSERT_EQ(restored.get_created_by(), long_user);
}

TEST(test_task_marshal_special_characters) {
    Task original(1, "Title with spaces & symbols!", "Description\twith\nnewlines", 
                  "board-special", "user@domain.com", Column::TODO, 1);
    
    int size = original.Size();
    char* buffer = new char[size];
    original.Marshal(buffer);
    
    Task restored;
    restored.Unmarshal(buffer);
    delete[] buffer;
    
    ASSERT_EQ(restored.get_title(), "Title with spaces & symbols!");
    ASSERT_EQ(restored.get_description(), "Description\twith\nnewlines");
    ASSERT_EQ(restored.get_created_by(), "user@domain.com");
}

TEST(test_task_marshal_unicode) {
    Task original(1, "タスク", "描述", "板-1", "用户", Column::TODO, 1);
    
    int size = original.Size();
    char* buffer = new char[size];
    original.Marshal(buffer);
    
    Task restored;
    restored.Unmarshal(buffer);
    delete[] buffer;
    
    ASSERT_EQ(restored.get_title(), "タスク");
    ASSERT_EQ(restored.get_description(), "描述");
}

TEST(test_task_marshal_negative_ids) {
    Task original(-1, "Title", "Desc", "board", "user", Column::TODO, -100);
    
    int size = original.Size();
    char* buffer = new char[size];
    original.Marshal(buffer);
    
    Task restored;
    restored.Unmarshal(buffer);
    delete[] buffer;
    
    ASSERT_EQ(restored.get_task_id(), -1);
    ASSERT_EQ(restored.get_client_id(), -100);
}

TEST(test_task_marshal_max_int_ids) {
    Task original(2147483647, "Title", "Desc", "board", "user", Column::TODO, 2147483647);
    
    int size = original.Size();
    char* buffer = new char[size];
    original.Marshal(buffer);
    
    Task restored;
    restored.Unmarshal(buffer);
    delete[] buffer;
    
    ASSERT_EQ(restored.get_task_id(), 2147483647);
    ASSERT_EQ(restored.get_client_id(), 2147483647);
}

TEST(test_task_marshal_with_vector_clock) {
    Task original(1, "Title", "Desc", "board", "user", Column::TODO, 100);
    original.get_clock().increment();
    original.get_clock().increment();
    original.get_clock().increment();
    
    int size = original.Size();
    char* buffer = new char[size];
    original.Marshal(buffer);
    
    Task restored;
    restored.Unmarshal(buffer);
    delete[] buffer;
    
    ASSERT_EQ(restored.get_clock().get(100), 3);
}

TEST(test_task_marshal_complex_vector_clock) {
    Task original(1, "Title", "Desc", "board", "user", Column::TODO, 1);
    
    // Simulate updates from multiple clients
    VectorClock vc2(2);
    vc2.increment();
    vc2.increment();
    original.get_clock().update(vc2);
    
    VectorClock vc3(3);
    vc3.increment();
    original.get_clock().update(vc3);
    
    int size = original.Size();
    char* buffer = new char[size];
    original.Marshal(buffer);
    
    Task restored;
    restored.Unmarshal(buffer);
    delete[] buffer;
    
    // Check all clock entries preserved
    ASSERT_TRUE(restored.get_clock().get(1) > 0);
    ASSERT_TRUE(restored.get_clock().get(2) >= 2);
    ASSERT_TRUE(restored.get_clock().get(3) >= 1);
}

TEST(test_task_timestamps_preserved) {
    Task original(1, "Title", "Desc", "board", "user", Column::TODO, 1);
    long long created = original.get_created_at();
    long long updated = original.get_updated_at();
    
    ASSERT_TRUE(created > 0);
    ASSERT_TRUE(updated > 0);
    
    int size = original.Size();
    char* buffer = new char[size];
    original.Marshal(buffer);
    
    Task restored;
    restored.Unmarshal(buffer);
    delete[] buffer;
    
    ASSERT_EQ(restored.get_created_at(), created);
    ASSERT_EQ(restored.get_updated_at(), updated);
}

/* ============ LogEntry Marshalling Tests ============ */

TEST(test_logentry_marshal_create_task) {
    VectorClock vc(1);
    vc.increment();
    
    LogEntry original(0, OpType::CREATE_TASK, vc, 5, "New Task", "Description", "alice", Column::TODO, 1);
    
    int size = original.Size();
    char* buffer = new char[size];
    original.Marshal(buffer);
    
    LogEntry restored(0, OpType::CREATE_TASK, VectorClock(0), 0, "", "", "", Column::TODO, 0);
    restored.Unmarshal(buffer);
    delete[] buffer;
    
    ASSERT_EQ(restored.get_entry_id(), 0);
    ASSERT_EQ(restored.get_op_type(), OpType::CREATE_TASK);
    ASSERT_EQ(restored.get_task_id(), 5);
    ASSERT_EQ(restored.get_title(), "New Task");
    ASSERT_EQ(restored.get_description(), "Description");
    ASSERT_EQ(restored.get_created_by(), "alice");
    ASSERT_EQ(restored.get_column(), Column::TODO);
    ASSERT_EQ(restored.get_client_id(), 1);
}

TEST(test_logentry_marshal_update_task) {
    VectorClock vc(2);
    vc.increment();
    vc.increment();
    
    LogEntry original(10, OpType::UPDATE_TASK, vc, 3, "Updated Title", "Updated Desc", "", Column::TODO, 2);
    
    int size = original.Size();
    char* buffer = new char[size];
    original.Marshal(buffer);
    
    LogEntry restored(0, OpType::CREATE_TASK, VectorClock(0), 0, "", "", "", Column::TODO, 0);
    restored.Unmarshal(buffer);
    delete[] buffer;
    
    ASSERT_EQ(restored.get_entry_id(), 10);
    ASSERT_EQ(restored.get_op_type(), OpType::UPDATE_TASK);
    ASSERT_EQ(restored.get_task_id(), 3);
    ASSERT_EQ(restored.get_title(), "Updated Title");
    ASSERT_EQ(restored.get_description(), "Updated Desc");
}

TEST(test_logentry_marshal_move_task) {
    VectorClock vc(1);
    
    LogEntry original(5, OpType::MOVE_TASK, vc, 7, "", "", "", Column::IN_PROGRESS, 1);
    
    int size = original.Size();
    char* buffer = new char[size];
    original.Marshal(buffer);
    
    LogEntry restored(0, OpType::CREATE_TASK, VectorClock(0), 0, "", "", "", Column::TODO, 0);
    restored.Unmarshal(buffer);
    delete[] buffer;
    
    ASSERT_EQ(restored.get_op_type(), OpType::MOVE_TASK);
    ASSERT_EQ(restored.get_task_id(), 7);
    ASSERT_EQ(restored.get_column(), Column::IN_PROGRESS);
}

TEST(test_logentry_marshal_delete_task) {
    VectorClock vc(3);
    
    LogEntry original(99, OpType::DELETE_TASK, vc, 42, "", "", "", Column::TODO, 3);
    
    int size = original.Size();
    char* buffer = new char[size];
    original.Marshal(buffer);
    
    LogEntry restored(0, OpType::CREATE_TASK, VectorClock(0), 0, "", "", "", Column::TODO, 0);
    restored.Unmarshal(buffer);
    delete[] buffer;
    
    ASSERT_EQ(restored.get_op_type(), OpType::DELETE_TASK);
    ASSERT_EQ(restored.get_task_id(), 42);
    ASSERT_EQ(restored.get_entry_id(), 99);
}

TEST(test_logentry_marshal_all_columns) {
    VectorClock vc(1);
    
    // Test TODO
    LogEntry e1(0, OpType::CREATE_TASK, vc, 0, "T", "D", "U", Column::TODO, 1);
    int size1 = e1.Size();
    char* buf1 = new char[size1];
    e1.Marshal(buf1);
    LogEntry r1(0, OpType::CREATE_TASK, VectorClock(0), 0, "", "", "", Column::TODO, 0);
    r1.Unmarshal(buf1);
    delete[] buf1;
    ASSERT_EQ(r1.get_column(), Column::TODO);
    
    // Test IN_PROGRESS
    LogEntry e2(1, OpType::CREATE_TASK, vc, 1, "T", "D", "U", Column::IN_PROGRESS, 1);
    int size2 = e2.Size();
    char* buf2 = new char[size2];
    e2.Marshal(buf2);
    LogEntry r2(0, OpType::CREATE_TASK, VectorClock(0), 0, "", "", "", Column::TODO, 0);
    r2.Unmarshal(buf2);
    delete[] buf2;
    ASSERT_EQ(r2.get_column(), Column::IN_PROGRESS);
    
    // Test DONE
    LogEntry e3(2, OpType::CREATE_TASK, vc, 2, "T", "D", "U", Column::DONE, 1);
    int size3 = e3.Size();
    char* buf3 = new char[size3];
    e3.Marshal(buf3);
    LogEntry r3(0, OpType::CREATE_TASK, VectorClock(0), 0, "", "", "", Column::TODO, 0);
    r3.Unmarshal(buf3);
    delete[] buf3;
    ASSERT_EQ(r3.get_column(), Column::DONE);
}

TEST(test_logentry_vector_clock_preserved) {
    VectorClock vc(1);
    vc.increment();
    vc.increment();
    
    VectorClock vc2(2);
    vc2.increment();
    vc.update(vc2);
    
    LogEntry original(0, OpType::CREATE_TASK, vc, 0, "T", "D", "U", Column::TODO, 1);
    
    int size = original.Size();
    char* buffer = new char[size];
    original.Marshal(buffer);
    
    LogEntry restored(0, OpType::CREATE_TASK, VectorClock(0), 0, "", "", "", Column::TODO, 0);
    restored.Unmarshal(buffer);
    delete[] buffer;
    
    const VectorClock& restored_vc = restored.get_timestamp();
    ASSERT_TRUE(restored_vc.get(1) >= 2);
    ASSERT_TRUE(restored_vc.get(2) >= 1);
}

/* ============ Size Calculation Tests ============ */

TEST(test_task_size_calculation) {
    Task t1(1, "A", "B", "C", "D", Column::TODO, 1);
    int size1 = t1.Size();
    
    Task t2(1, "AAAA", "BBBB", "CCCC", "DDDD", Column::TODO, 1);
    int size2 = t2.Size();
    
    // Longer strings should result in larger size
    ASSERT_TRUE(size2 > size1);
}

TEST(test_logentry_size_calculation) {
    VectorClock vc(1);
    
    LogEntry e1(0, OpType::CREATE_TASK, vc, 0, "A", "B", "C", Column::TODO, 1);
    int size1 = e1.Size();
    
    LogEntry e2(0, OpType::CREATE_TASK, vc, 0, "AAAA", "BBBB", "CCCC", Column::TODO, 1);
    int size2 = e2.Size();
    
    ASSERT_TRUE(size2 > size1);
}

/* ============ Multiple Marshal/Unmarshal Cycles ============ */

TEST(test_task_multiple_cycles) {
    Task original(1, "Title", "Description", "board-1", "user", Column::IN_PROGRESS, 42);
    
    // Cycle 1
    int size1 = original.Size();
    char* buf1 = new char[size1];
    original.Marshal(buf1);
    Task t1;
    t1.Unmarshal(buf1);
    delete[] buf1;
    
    // Cycle 2
    int size2 = t1.Size();
    char* buf2 = new char[size2];
    t1.Marshal(buf2);
    Task t2;
    t2.Unmarshal(buf2);
    delete[] buf2;
    
    // Cycle 3
    int size3 = t2.Size();
    char* buf3 = new char[size3];
    t2.Marshal(buf3);
    Task t3;
    t3.Unmarshal(buf3);
    delete[] buf3;
    
    // Final result should match original
    ASSERT_EQ(t3.get_task_id(), original.get_task_id());
    ASSERT_EQ(t3.get_title(), original.get_title());
    ASSERT_EQ(t3.get_description(), original.get_description());
    ASSERT_EQ(t3.get_board_id(), original.get_board_id());
    ASSERT_EQ(t3.get_created_by(), original.get_created_by());
    ASSERT_EQ(t3.get_column(), original.get_column());
    ASSERT_EQ(t3.get_client_id(), original.get_client_id());
}

/* ============ Main ============ */

int main() {
    std::cout << "==========================================\n";
    std::cout << "Running Marshalling/Unmarshalling Tests\n";
    std::cout << "==========================================\n\n";
    
    std::cout << "--- Task Marshalling Tests ---\n";
    RUN_TEST(test_task_marshal_unmarshal_basic);
    RUN_TEST(test_task_marshal_unmarshal_in_progress);
    RUN_TEST(test_task_marshal_unmarshal_done);
    RUN_TEST(test_task_marshal_empty_strings);
    RUN_TEST(test_task_marshal_long_strings);
    RUN_TEST(test_task_marshal_special_characters);
    RUN_TEST(test_task_marshal_unicode);
    RUN_TEST(test_task_marshal_negative_ids);
    RUN_TEST(test_task_marshal_max_int_ids);
    RUN_TEST(test_task_marshal_with_vector_clock);
    RUN_TEST(test_task_marshal_complex_vector_clock);
    RUN_TEST(test_task_timestamps_preserved);
    
    std::cout << "\n--- LogEntry Marshalling Tests ---\n";
    RUN_TEST(test_logentry_marshal_create_task);
    RUN_TEST(test_logentry_marshal_update_task);
    RUN_TEST(test_logentry_marshal_move_task);
    RUN_TEST(test_logentry_marshal_delete_task);
    RUN_TEST(test_logentry_marshal_all_columns);
    RUN_TEST(test_logentry_vector_clock_preserved);
    
    std::cout << "\n--- Size Calculation Tests ---\n";
    RUN_TEST(test_task_size_calculation);
    RUN_TEST(test_logentry_size_calculation);
    
    std::cout << "\n--- Multiple Cycle Tests ---\n";
    RUN_TEST(test_task_multiple_cycles);
    
    std::cout << "\n==========================================\n";
    std::cout << "Results: " << tests_passed << " passed, " << tests_failed << " failed\n";
    std::cout << "==========================================\n";
    
    return tests_failed > 0 ? 1 : 0;
}
