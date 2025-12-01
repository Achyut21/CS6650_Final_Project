#ifndef __TASK_MANAGER_H__
#define __TASK_MANAGER_H__

#include <map>
#include <vector>
#include <mutex>
#include "messages.h"

class TaskManager
{
private:
    int id_counter;
    std::map<int, Task> tasks;
    std::mutex task_lock;
    std::vector<VectorClock *> clocks;

public:
    TaskManager();
    // New signature with all fields including column
    bool create_task(std::string title, std::string description, std::string board_id, 
                     std::string created_by, Column column, int client_id);
    // Backward compatible signature for tests
    bool create_task(std::string description, int client_id);
    
    OperationResponse update_task_with_conflict_detection(int task_id, const std::string &title, const std::string &description, const VectorClock &vc);
    OperationResponse move_task_with_conflict_detection(int task_id, Column column, const VectorClock &vc);
    bool update_task(int task_id, const std::string &title, const std::string &description, const VectorClock &vc);
    bool move_task(int task_id, Column column, const VectorClock &vc);
    bool delete_task(int task_id);
    Task get_task(int id);
    std::vector<Task> get_all_tasks();

    // SMR methods
    void append_to_log(const LogEntry &entry);
    std::vector<LogEntry> get_log() const;
    std::vector<LogEntry> get_log_after(int entry_id) const;
    void replay_log(const std::vector<LogEntry> &entries);
    void merge_logs(const std::vector<LogEntry> &remote_log);

    size_t get_task_count() const;
    
    // State transfer methods for master rejoin
    void clear_all_tasks();  // Clear all tasks (for receiving state transfer)
    void set_id_counter(int id);  // Set next task ID
    int get_id_counter() const;  // Get current ID counter
    void add_task_direct(const Task& task);  // Add task without incrementing counter
};

#endif