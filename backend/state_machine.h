#ifndef __STATE_MACHINE_H__
#define __STATE_MACHINE_H__

#include <vector>
#include <mutex>
#include "messages.h"
#include "task_manager.h"

// State machine log for operation logging and replay
class StateMachine {
private:
    std::vector<LogEntry> log;
    mutable std::mutex log_mutex;
    int next_entry_id;

public:
    StateMachine();
    
    // Append operation to log
    void append_to_log(const LogEntry& entry);
    
    // Get entire log
    std::vector<LogEntry> get_log() const;
    
    // Get log entries after given id
    std::vector<LogEntry> get_log_after(int entry_id) const;
    
    // Replay log entries on TaskManager
    void replay_log(TaskManager& tm, const std::vector<LogEntry>& entries);
    
    // Get log size
    size_t get_log_size() const;
};

#endif
