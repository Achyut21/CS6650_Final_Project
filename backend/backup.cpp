#include <iostream>
#include <thread>
#include <chrono>
#include <csignal>
#include "Socket.h"
#include "ServerStub.h"
#include "ClientStub.h"
#include "task_manager.h"
#include "state_machine.h"
#include "messages.h"

// Global variables
TaskManager task_manager;
StateMachine state_machine;
bool server_running = true;
bool is_promoted = false;
int backup_port = 12346;  // Updated from argv in main()
int next_entry_id = 0;    // Track next entry ID for log
Socket* global_server_socket = nullptr;
std::map<int, VectorClock> client_clocks;  // Track vector clock per client after promotion
std::mutex clock_mutex;
std::mutex promotion_mutex;  // Protect is_promoted flag

void SignalHandler(int) {
    std::cout << "\nShutting down backup...\n";
    server_running = false;
    // Close server socket to unblock accept()
    if (global_server_socket) {
        global_server_socket->Close();
    }
}

// Try to rejoin after restart - connect to master and request current state
// Returns true if state was received from master
bool TryRejoinFromMaster(const std::string& master_ip, int master_port) {
    ClientStub client;
    if (!client.Init(master_ip, master_port)) {
        // Master not reachable - this is normal on first start
        return false;
    }
    
    std::cout << "[REJOIN] Connected to master, requesting state sync\n";
    
    // Send STATE_TRANSFER_REQUEST
    if (!client.SendOpType(OpType::STATE_TRANSFER_REQUEST)) {
        std::cerr << "[REJOIN] Failed to send STATE_TRANSFER_REQUEST\n";
        client.Close();
        return false;
    }
    
    // Receive state transfer
    std::vector<Task> tasks;
    std::vector<LogEntry> log;
    int id_counter;
    
    if (!client.ReceiveStateTransfer(tasks, log, id_counter)) {
        std::cerr << "[REJOIN] Failed to receive state from master\n";
        client.Close();
        return false;
    }
    
    std::cout << "[REJOIN] Received: " << tasks.size() << " tasks, " 
              << log.size() << " log entries, ID counter: " << id_counter << "\n";
    
    // Apply state
    task_manager.clear_all_tasks();
    for (const Task& task : tasks) {
        task_manager.add_task_direct(task);
    }
    task_manager.set_id_counter(id_counter);
    
    // Apply log
    state_machine.set_log(log);
    next_entry_id = state_machine.get_next_entry_id();
    
    std::cout << "[REJOIN] State applied successfully, next entry ID: " << next_entry_id << "\n";
    
    client.Close();
    return true;
}

// Handle client requests after promotion (same as master but no replication)
void HandleClient(Socket* client_socket, int client_id) {
    ServerStub stub;
    if (!stub.Init(client_socket)) {
        delete client_socket;
        return;
    }
    
    std::cout << "Client " << client_id << " connected to promoted backup\n";
    
    while (true) {
        OpType op_type = stub.ReceiveOpType();
        
        if (static_cast<int>(op_type) == -1) {
            break;
        }
        
        Task task = stub.ReceiveTask();
        bool success = false;
        OperationResponse op_response;
        
        switch (op_type) {
            case OpType::CREATE_TASK:
                success = task_manager.create_task(
                    task.get_title(),
                    task.get_description(),
                    task.get_board_id(),
                    task.get_created_by(),
                    task.get_column(),
                    task.get_client_id()
                );
                
                // Send OperationResponse with task ID (use id_counter, not task_count)
                op_response.success = success;
                op_response.conflict = false;
                op_response.rejected = false;
                op_response.updated_task_id = success ? (task_manager.get_id_counter() - 1) : -1;
                
                if (success) std::cout << "Created task " << op_response.updated_task_id << " (promoted backup)\n";
                
                stub.SendOperationResponse(op_response);
                continue; // Skip default SendSuccess
                
            case OpType::UPDATE_TASK:
                {
                    VectorClock vc(client_id);
                    std::lock_guard<std::mutex> lock(clock_mutex);
                    auto it = client_clocks.find(client_id);
                    if (it != client_clocks.end()) {
                        vc = it->second;
                        vc.increment();
                        it->second = vc;
                    } else {
                        client_clocks.insert({client_id, vc});
                    }
                    op_response = task_manager.update_task_with_conflict_detection(
                        task.get_task_id(), task.get_title(), task.get_description(), vc);
                }
                if (op_response.success) std::cout << "Updated task " << task.get_task_id() << "\n";
                
                stub.SendOperationResponse(op_response);
                continue; // Skip default SendSuccess
                
            case OpType::MOVE_TASK:
                {
                    VectorClock vc(client_id);
                    std::lock_guard<std::mutex> lock(clock_mutex);
                    auto it = client_clocks.find(client_id);
                    if (it != client_clocks.end()) {
                        vc = it->second;
                        vc.increment();
                        it->second = vc;
                    } else {
                        client_clocks.insert({client_id, vc});
                    }
                    op_response = task_manager.move_task_with_conflict_detection(
                        task.get_task_id(), task.get_column(), vc);
                }
                if (op_response.success) std::cout << "Moved task " << task.get_task_id() << "\n";
                
                stub.SendOperationResponse(op_response);
                continue; // Skip default SendSuccess
                
            case OpType::DELETE_TASK:
                success = task_manager.delete_task(task.get_task_id());
                if (success) std::cout << "Deleted task " << task.get_task_id() << "\n";
                break;
                
            case OpType::GET_BOARD: {
                std::vector<Task> all_tasks = task_manager.get_all_tasks();
                std::cout << "GET_BOARD request - returning " << all_tasks.size() << " tasks\n";
                stub.SendTaskList(all_tasks);
                continue; // Skip SendSuccess
            }
            
            case OpType::HEARTBEAT_PING:
            case OpType::HEARTBEAT_ACK:
            case OpType::MASTER_REJOIN:
            case OpType::STATE_TRANSFER_REQUEST:
            case OpType::STATE_TRANSFER_RESPONSE:
            case OpType::DEMOTE_ACK:
            case OpType::REPLICATION_INIT:
                // These shouldn't come through HandleClient
                std::cerr << "Unexpected control message in HandleClient\n";
                break;
                
            default:
                break;
        }
        
        stub.SendSuccess(success);
    }
    
    std::cout << "Client " << client_id << " disconnected\n";
    delete client_socket;
}

// Handle master rejoin - send state and demote
bool HandleMasterRejoin(Socket* client_socket) {
    ServerStub stub;
    if (!stub.Init(client_socket)) {
        delete client_socket;
        return false;
    }
    
    std::cout << "[MASTER REJOIN] Master is rejoining\n";
    
    // Get current state
    std::vector<Task> tasks = task_manager.get_all_tasks();
    std::vector<LogEntry> log = state_machine.get_log();
    int id_counter = task_manager.get_id_counter();
    
    std::cout << "[STATE TRANSFER] Sending to master: " << tasks.size() << " tasks, "
              << log.size() << " log entries, ID counter: " << id_counter << "\n";
    
    // Send state transfer
    if (!stub.SendStateTransfer(tasks, log, id_counter)) {
        std::cerr << "[STATE TRANSFER] Failed to send state to master\n";
        delete client_socket;
        return false;
    }
    
    std::cout << "[STATE TRANSFER] State sent successfully\n";
    
    // Wait for DEMOTE_ACK from master
    OpType ack = stub.ReceiveOpType();
    if (ack != OpType::DEMOTE_ACK) {
        std::cerr << "[STATE TRANSFER] Did not receive DEMOTE_ACK, got: " << static_cast<int>(ack) << "\n";
        delete client_socket;
        return false;
    }
    
    std::cout << "[DEMOTE] Received DEMOTE_ACK from master\n";
    
    // Demote back to backup mode
    {
        std::lock_guard<std::mutex> lock(promotion_mutex);
        is_promoted = false;
    }
    
    std::cout << "[DEMOTE] Backup demoted, returning to backup mode\n";
    
    delete client_socket;
    return true;
}

// Handle replication from primary
void HandleReplication(Socket* client_socket) {
    ServerStub stub;
    if (!stub.Init(client_socket)) {
        delete client_socket;
        return;
    }
    
    std::cout << "Primary connected for replication\n";
    
    // First message should be REPLICATION_INIT handshake
    OpType first_op = stub.ReceiveOpType();
    if (first_op != OpType::REPLICATION_INIT) {
        std::cout << "[BACKUP MODE] Expected REPLICATION_INIT but got optype " 
                  << static_cast<int>(first_op) << " - rejecting connection\n";
        stub.SendSuccess(false);
        delete client_socket;
        return;
    }
    
    // Acknowledge the handshake
    std::cout << "[BACKUP MODE] Received REPLICATION_INIT - acknowledged\n";
    stub.SendSuccess(true);
    
    while (true) {
        // Receive operation type
        OpType op_type = stub.ReceiveOpType();
        
        if (static_cast<int>(op_type) == -1) {
            std::cout << "ReceiveOpType failed - Primary disconnected" << std::endl;
            std::cout << "PROMOTING TO MASTER" << std::endl;
            {
                std::lock_guard<std::mutex> lock(promotion_mutex);
                is_promoted = true;
            }
            std::cout << "Backup promoted! Now accepting client connections on port " << backup_port << std::endl;
            std::cout << "Total tasks replicated: " << task_manager.get_task_count() << std::endl;
            std::cout << "State machine log size: " << state_machine.get_log_size() << std::endl;
            std::cout.flush();
            break;
        }
        
        // Handle heartbeat separately
        if (op_type == OpType::HEARTBEAT_PING) {
            // Respond with HEARTBEAT_ACK
            if (!stub.SendSuccess(true)) {  // Send ack (reusing SendSuccess)
                std::cout << "Failed to send heartbeat ack - Primary disconnected\n";
                break;
            }
            std::cout << "[HEARTBEAT] Received ping, sent ack\n";
            continue; // Go to next iteration
        }
        
        // Handle unexpected MASTER_REJOIN when we're not promoted
        // This happens when master restarts and tries to rejoin, but we never promoted
        if (op_type == OpType::MASTER_REJOIN) {
            std::cout << "[BACKUP MODE] Received MASTER_REJOIN but not promoted - rejecting\n";
            // Send failure response so master knows to start fresh
            stub.SendSuccess(false);
            // Close this connection gracefully, don't promote
            break;  // Exit HandleReplication WITHOUT setting is_promoted = true
        }
        
        // For task operations from master, receive the log entry
        // Note: We already verified this is a master connection via REPLICATION_INIT handshake,
        // so CREATE_TASK/UPDATE_TASK/etc. here are valid replication operations (not gateway mistakes)
        LogEntry entry = stub.ReceiveLogEntry();
        
        // Check for disconnect (entry_id == -1 indicates error)
        if (entry.get_entry_id() < 0) {
            std::cout << "ReceiveLogEntry failed - Primary disconnected" << std::endl;
            std::cout << "PROMOTING TO MASTER" << std::endl;
            {
                std::lock_guard<std::mutex> lock(promotion_mutex);
                is_promoted = true;
            }
            std::cout << "Backup promoted! Now accepting client connections on port " << backup_port << std::endl;
            std::cout << "Total tasks replicated: " << task_manager.get_task_count() << std::endl;
            std::cout << "State machine log size: " << state_machine.get_log_size() << std::endl;
            std::cout.flush();
            break;
        }
        
        // Append to state machine log
        state_machine.append_to_log(entry);
        next_entry_id = entry.get_entry_id() + 1;
        
        // Apply operation to task manager with vector clock
        OpType op = entry.get_op_type();
        const VectorClock& vc = entry.get_timestamp();
        
        switch (op) {
            case OpType::CREATE_TASK:
                // Debug: Log the column value from the entry
                std::cout << "[DEBUG] CREATE_TASK replication - title: " << entry.get_title()
                          << ", created_by: " << entry.get_created_by()
                          << ", column: " << static_cast<int>(entry.get_column()) << "\n";
                // Use full create_task with all fields from log entry
                task_manager.create_task(entry.get_title(), entry.get_description(), "board-1", entry.get_created_by(),
                                        entry.get_column(), entry.get_client_id());
                std::cout << "Replicated CREATE_TASK (title: " << entry.get_title() 
                          << ", created_by: " << entry.get_created_by()
                          << ", column: " << static_cast<int>(entry.get_column()) << ")\n";
                break;
                
            case OpType::UPDATE_TASK:
                task_manager.update_task(entry.get_task_id(), entry.get_title(), entry.get_description(), vc);
                std::cout << "Replicated UPDATE_TASK\n";
                break;
                
            case OpType::MOVE_TASK:
                task_manager.move_task(entry.get_task_id(), entry.get_column(), vc);
                std::cout << "Replicated MOVE_TASK\n";
                break;
                
            case OpType::DELETE_TASK:
                task_manager.delete_task(entry.get_task_id());
                std::cout << "Replicated DELETE_TASK\n";
                break;
                
            case OpType::GET_BOARD:
                // GET_BOARD is not a state-changing operation, skip in replication
                break;
                
            case OpType::HEARTBEAT_PING:
            case OpType::HEARTBEAT_ACK:
            case OpType::MASTER_REJOIN:
            case OpType::STATE_TRANSFER_REQUEST:
            case OpType::STATE_TRANSFER_RESPONSE:
            case OpType::DEMOTE_ACK:
            case OpType::REPLICATION_INIT:
                // Control messages handled separately, not logged
                break;
        }
        
        // Send acknowledgment
        if (!stub.SendSuccess(true)) {
            std::cout << "Failed to send ack to primary" << std::endl;
            std::cout << "Primary disconnected - PROMOTING TO MASTER" << std::endl;
            {
                std::lock_guard<std::mutex> lock(promotion_mutex);
                is_promoted = true;
            }
            std::cout << "Backup promoted! Now accepting client connections on port " << backup_port << std::endl;
            std::cout << "Total tasks replicated: " << task_manager.get_task_count() << std::endl;
            std::cout << "State machine log size: " << state_machine.get_log_size() << std::endl;
            std::cout.flush();
            break;
        }
    }
    
    delete client_socket;
}


int main(int argc, char* argv[]) {
    // Parse: ./backup [port] [node_id] [primary_ip] [primary_port]
    if (argc != 5) {
        std::cerr << "Usage: ./backup [port] [node_id] [primary_ip] [primary_port]\n";
        return 1;
    }
    
    int port = std::stoi(argv[1]);
    backup_port = port;  // Sync global for promotion messages
    int node_id = std::stoi(argv[2]);
    std::string primary_ip = argv[3];
    int primary_port = std::stoi(argv[4]);
    
    std::cout << "Starting backup node " << node_id << " on port " << port << "\n";
    std::cout << "Primary: " << primary_ip << ":" << primary_port << "\n";
    
    // Try to rejoin from master (in case we crashed and master has newer state)
    bool rejoined = TryRejoinFromMaster(primary_ip, primary_port);
    if (rejoined) {
        std::cout << "Recovered state from master\n";
    } else {
        std::cout << "Starting fresh (master not reachable or no state to sync)\n";
    }
    
    signal(SIGINT, SignalHandler);
    
    // Create server socket
    Socket server_socket;
    global_server_socket = &server_socket;
    
    if (!server_socket.Bind(port)) {
        std::cerr << "Failed to bind to port " << port << "\n";
        return 1;
    }
    
    if (!server_socket.Listen()) {
        std::cerr << "Failed to listen\n";
        return 1;
    }
    
    std::cout << "Backup listening on port " << port << "...\n";
    std::cout << "Waiting for primary connection or ready to promote...\n";
    
    // Accept connections (from primary for replication OR from clients after promotion)
    while (server_running) {
        bool currently_promoted;
        {
            std::lock_guard<std::mutex> lock(promotion_mutex);
            currently_promoted = is_promoted;
        }
        
        if (currently_promoted) {
            std::cout << "[PROMOTED MODE] Waiting for connections (clients or master rejoin)..." << std::endl;
        }
        
        Socket* socket = server_socket.Accept();
        
        if (socket && socket->IsValid()) {
            // Check current promotion state
            {
                std::lock_guard<std::mutex> lock(promotion_mutex);
                currently_promoted = is_promoted;
            }
            
            if (!currently_promoted) {
                // Still backup - handle replication from master
                std::cout << "[BACKUP MODE] Handling replication connection" << std::endl;
                std::thread(HandleReplication, socket).detach();
            } else {
                // Promoted mode - need to check if this is master rejoining or a client
                // Peek at first OpType to determine
                ServerStub peek_stub;
                if (!peek_stub.Init(socket)) {
                    delete socket;
                    continue;
                }
                
                OpType first_op = peek_stub.ReceiveOpType();
                
                if (first_op == OpType::MASTER_REJOIN) {
                    // Master is rejoining - handle state transfer and demote
                    std::cout << "[PROMOTED MODE] Master rejoin detected!" << std::endl;
                    if (HandleMasterRejoin(socket)) {
                        std::cout << "[BACKUP MODE] Successfully demoted, resuming backup mode\n";
                    } else {
                        std::cout << "[PROMOTED MODE] Master rejoin failed, staying promoted\n";
                    }
                    continue;  // Go back to Accept() - socket already handled
                } else if (first_op == OpType::REPLICATION_INIT) {
                    // Master is trying to replicate but we're promoted!
                    // This shouldn't happen - master should use MASTER_REJOIN
                    std::cout << "[PROMOTED MODE] Received REPLICATION_INIT but I'm promoted!\n";
                    std::cout << "[PROMOTED MODE] Master should use MASTER_REJOIN instead.\n";
                    std::cout << "[PROMOTED MODE] Rejecting connection - master will retry with MASTER_REJOIN.\n";
                    peek_stub.SendSuccess(false);  // Reject the handshake
                    delete socket;
                    continue;  // Go back to Accept()
                } else if (static_cast<int>(first_op) == -1) {
                    // Connection closed immediately
                    delete socket;
                    continue;  // Go back to Accept()
                } else {
                    // It's a client connection - but we already consumed the OpType!
                    // We need to handle this request inline
                    std::cout << "[PROMOTED MODE] Client connection (first op: " << static_cast<int>(first_op) << ")" << std::endl;
                    
                    // Create a wrapper to handle this connection with the pre-read OpType
                    // For simplicity, handle inline here
                    Task task = peek_stub.ReceiveTask();
                    bool success = false;
                    OperationResponse op_response;
                    
                    switch (first_op) {
                        case OpType::CREATE_TASK:
                            success = task_manager.create_task(
                                task.get_title(), task.get_description(),
                                task.get_board_id(), task.get_created_by(),
                                task.get_column(), task.get_client_id()
                            );
                            op_response.success = success;
                            op_response.updated_task_id = success ? (task_manager.get_id_counter() - 1) : -1;
                            peek_stub.SendOperationResponse(op_response);
                            break;
                            
                        case OpType::UPDATE_TASK: {
                            // Use proper vector clock tracking
                            VectorClock vc(1);  // Use client_id 1 for gateway connections
                            {
                                std::lock_guard<std::mutex> lock(clock_mutex);
                                auto it = client_clocks.find(1);
                                if (it != client_clocks.end()) {
                                    vc = it->second;
                                    vc.increment();
                                    it->second = vc;
                                } else {
                                    client_clocks.insert({1, vc});
                                }
                            }
                            op_response = task_manager.update_task_with_conflict_detection(
                                task.get_task_id(), task.get_title(), task.get_description(), vc);
                            peek_stub.SendOperationResponse(op_response);
                            break;
                        }
                            
                        case OpType::MOVE_TASK: {
                            // Use proper vector clock tracking
                            VectorClock vc(1);  // Use client_id 1 for gateway connections
                            {
                                std::lock_guard<std::mutex> lock(clock_mutex);
                                auto it = client_clocks.find(1);
                                if (it != client_clocks.end()) {
                                    vc = it->second;
                                    vc.increment();
                                    it->second = vc;
                                } else {
                                    client_clocks.insert({1, vc});
                                }
                            }
                            op_response = task_manager.move_task_with_conflict_detection(
                                task.get_task_id(), task.get_column(), vc);
                            peek_stub.SendOperationResponse(op_response);
                            break;
                        }
                            
                        case OpType::DELETE_TASK:
                            success = task_manager.delete_task(task.get_task_id());
                            peek_stub.SendSuccess(success);
                            break;
                            
                        case OpType::GET_BOARD: {
                            std::vector<Task> all_tasks = task_manager.get_all_tasks();
                            peek_stub.SendTaskList(all_tasks);
                            break;
                        }
                            
                        default:
                            peek_stub.SendSuccess(false);
                            break;
                    }
                    
                    // Gateway closes connection after each request, so just close socket
                    // No need to spawn a thread that will immediately exit
                    delete socket;
                }
            }
        } else if (!server_running) {
            // Server shutting down
            break;
        }
    }
    
    std::cout << "Backup shutdown complete\n";
    return 0;
}
