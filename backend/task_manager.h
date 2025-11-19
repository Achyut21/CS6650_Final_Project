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
    bool create_task(std::string description, int client_id);
    bool update_task(int task_id, const std::string &description);
    bool move_task(int task_id, Column column);
    bool delete_task(int task_id);
    Task get_task(int id);

    // SMR methods
    void append_to_log(const LogEntry &entry);
    std::vector<LogEntry> get_log() const;
    std::vector<LogEntry> get_log_after(int entry_id) const;
    void replay_log(const std::vector<LogEntry> &entries);
    void merge_logs(const std::vector<LogEntry> &remote_log);

    size_t get_task_count() const;
};

#endif