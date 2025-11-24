#include "state_machine.h"
#include <algorithm>

StateMachine::StateMachine() : next_entry_id(0) {}

// Append operation to log
void StateMachine::append_to_log(const LogEntry& entry) {
    std::lock_guard<std::mutex> lock(log_mutex);
    log.push_back(entry);
    next_entry_id++;
}

// Get entire log (thread-safe copy)
std::vector<LogEntry> StateMachine::get_log() const {
    std::lock_guard<std::mutex> lock(log_mutex);
    return log;
}

// Get log entries after given entry_id
std::vector<LogEntry> StateMachine::get_log_after(int entry_id) const {
    std::lock_guard<std::mutex> lock(log_mutex);
    std::vector<LogEntry> result;
    
    for (const auto& entry : log) {
        if (entry.get_entry_id() > entry_id) {
            result.push_back(entry);
        }
    }
    
    return result;
}

// Replay log entries on TaskManager with vector clock conflict detection
void StateMachine::replay_log(TaskManager& tm, const std::vector<LogEntry>& entries) {
    for (const auto& entry : entries) {
        OpType op = entry.get_op_type();
        const VectorClock& vc = entry.get_timestamp();
        
        switch (op) {
            case OpType::CREATE_TASK:
                // Use backward compatible version during replay
                tm.create_task(entry.get_description(), entry.get_client_id());
                break;
                
            case OpType::UPDATE_TASK:
                tm.update_task(entry.get_task_id(), entry.get_description(), vc);
                break;
                
            case OpType::MOVE_TASK:
                tm.move_task(entry.get_task_id(), entry.get_column(), vc);
                break;
                
            case OpType::DELETE_TASK:
                tm.delete_task(entry.get_task_id());
                break;
                
            case OpType::GET_BOARD:
                // GET_BOARD is not a state-changing operation, skip in replay
                break;
        }
    }
}

// Get log size
size_t StateMachine::get_log_size() const {
    std::lock_guard<std::mutex> lock(log_mutex);
    return log.size();
}
