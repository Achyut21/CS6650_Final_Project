#ifndef __STATE_MACHINE_H__
#define __STATE_MACHINE_H__

#include <vector>
#include <mutex>
#include "messages.h"
#include "task_manager.h"
class StateMachine
{
private:
    std::vector<LogEntry> smr_log;
    mutable std::mutex state_mutex;

    int last_index;
    int committed_index;

    int server_id;
    int primary_id;

    TaskManager *task_manager;

    // Private helper - no locking
    void apply_entry_internal(int index);

public:
    StateMachine(int id, TaskManager *tm);

    // Log Management
    void append_to_log(const LogEntry &entry);
    void apply_entry(int index);
    void commit_up_to(int index);

    LogEntry get_log_entry(int index) const;
    std::vector<LogEntry> get_log() const;
    std::vector<LogEntry> get_log_after(int index) const;

    // Replay
    void replay_log();
    void replay_from(int start_index);

    // Getters
    int get_last_index();
    int get_committed_index();
    int get_primary_id();
    int get_server_id();

    // Setters
    void set_primary_id(int id);

    // Primary-Backup Specific
    bool is_primary() const;
};
#endif