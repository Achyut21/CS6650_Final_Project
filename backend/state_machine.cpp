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
                // Use full create_task with all fields from log entry
                tm.create_task(entry.get_title(), entry.get_description(), "board-1", entry.get_created_by(),
                              entry.get_column(), entry.get_client_id());
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
                
            case OpType::HEARTBEAT_PING:
            case OpType::HEARTBEAT_ACK:
            case OpType::MASTER_REJOIN:
            case OpType::STATE_TRANSFER_REQUEST:
            case OpType::STATE_TRANSFER_RESPONSE:
            case OpType::DEMOTE_ACK:
            case OpType::REPLICATION_INIT:
                // Control messages are not state-changing, skip
                break;
        }
    }
}

// Get log size
size_t StateMachine::get_log_size() const {
    std::lock_guard<std::mutex> lock(log_mutex);
    return log.size();
}

// State transfer methods for master rejoin
void StateMachine::set_log(const std::vector<LogEntry>& new_log) {
    std::lock_guard<std::mutex> lock(log_mutex);
    log = new_log;
    if (!log.empty()) {
        next_entry_id = log.back().get_entry_id() + 1;
    } else {
        next_entry_id = 0;
    }
}

void StateMachine::clear_log() {
    std::lock_guard<std::mutex> lock(log_mutex);
    log.clear();
    next_entry_id = 0;
}

int StateMachine::get_next_entry_id() const {
    std::lock_guard<std::mutex> lock(log_mutex);
    return next_entry_id;
}

void StateMachine::set_next_entry_id(int id) {
    std::lock_guard<std::mutex> lock(log_mutex);
    next_entry_id = id;
}
