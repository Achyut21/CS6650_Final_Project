#include <iostream>
#include <thread>
#include <vector>
#include <csignal>
#include "Socket.h"
#include "ServerStub.h"
#include "ClientStub.h"
#include "task_manager.h"
#include "state_machine.h"
#include "replication.h"
#include "messages.h"

// Global variables
TaskManager task_manager;
StateMachine state_machine;
ReplicationManager* replication_manager = nullptr;
bool server_running = true;
int next_entry_id = 0;
Socket* global_server_socket = nullptr;
std::map<int, VectorClock> client_clocks;  // Track vector clock per client
std::mutex clock_mutex;

void SignalHandler(int) {
    std::cout << "\nShutting down server...\n";
    server_running = false;
    // Close server socket to unblock accept()
    if (global_server_socket) {
        global_server_socket->Close();
    }
}

// Try to rejoin after crash which is connect to backup, get state if it's promoted
// Returns true if state was received from promoted backup
bool TryRejoinFromBackup(const std::string& backup_ip, int backup_port) {
    ClientStub client;
    if (!client.Init(backup_ip, backup_port)) {
        // Backup not reachable - this is normal on first start
        return false;
    }
    
    // Send MASTER_REJOIN opcode to check if backup is promoted
    if (!client.SendOpType(OpType::MASTER_REJOIN)) {
        client.Close();
        return false;
    }
    
    // Try to receive state transfer
    std::vector<Task> tasks;
    std::vector<LogEntry> log;
    int id_counter;
    
    if (!client.ReceiveStateTransfer(tasks, log, id_counter)) {
        // Backup is not promoted and this is expected behaviour on first start
        client.Close();
        return false;
    }
    
    // Backup WAS promoted and we are actually rejoining!
    std::cout << "[REJOIN] Backup was promoted, receiving state transfer\n";
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
    
    std::cout << "[REJOIN] State applied, next entry ID: " << next_entry_id << "\n";
    
    // Send DEMOTE_ACK to backup
    if (!client.SendOpType(OpType::DEMOTE_ACK)) {
        std::cerr << "[REJOIN] Failed to send DEMOTE_ACK\n";
        client.Close();
        return false;
    }
    
    std::cout << "[REJOIN] Sent DEMOTE_ACK, backup demoting\n";
    
    client.Close();
    return true;
}

// Handle client requests in separate thread
void HandleClient(Socket* client_socket, int client_id) {
    ServerStub stub;
    if (!stub.Init(client_socket)) {
        delete client_socket;
        return;
    }
    
    std::cout << "Client " << client_id << " connected\n";
    
    while (true) {
        // Receive operation type
        OpType op_type = stub.ReceiveOpType();
        
        if (static_cast<int>(op_type) == -1) {
            // Client disconnected
            break;
        }
        
        // Handle STATE_TRANSFER_REQUEST before receiving task (backup rejoin doesn't send task)
        if (op_type == OpType::STATE_TRANSFER_REQUEST) {
            std::cout << "[STATE_TRANSFER] Backup requesting state sync\n";
            std::vector<Task> all_tasks = task_manager.get_all_tasks();
            std::vector<LogEntry> log = state_machine.get_log();
            int id_counter = task_manager.get_id_counter();
            
            std::cout << "[STATE_TRANSFER] Sending " << all_tasks.size() << " tasks, "
                      << log.size() << " log entries, ID counter: " << id_counter << "\n";
            
            if (!stub.SendStateTransfer(all_tasks, log, id_counter)) {
                std::cerr << "[STATE_TRANSFER] Failed to send state to backup\n";
            }
            continue;
        }
        
        // Receive task data
        Task task = stub.ReceiveTask();
        bool success = false;
        OperationResponse op_response;
        
        // Process based on operation type
        switch (op_type) {
            case OpType::CREATE_TASK: {
                // Get or create vector clock for this client
                VectorClock vc(client_id);
                {
                    std::lock_guard<std::mutex> clock_lock(clock_mutex);
                    auto clock_it = client_clocks.find(client_id);
                    if (clock_it != client_clocks.end()) {
                        vc = clock_it->second;
                        vc.increment();
                        clock_it->second = vc;
                    } else {
                        client_clocks.insert({client_id, vc});
                    }
                }
                
                success = task_manager.create_task(
                    task.get_title(),
                    task.get_description(),
                    task.get_board_id(),
                    task.get_created_by(),
                    task.get_column(),
                    task.get_client_id()
                );
                
                // Prepare response with created task ID (use id_counter, not task_count)
                op_response.success = success;
                op_response.conflict = false;
                op_response.rejected = false;
                op_response.updated_task_id = success ? (task_manager.get_id_counter() - 1) : -1;
                
                if (success) {
                    // Debug: Log the column being replicated
                    std::cout << "[DEBUG] CREATE_TASK - column from task: " 
                              << static_cast<int>(task.get_column()) << "\n";
                    
                    // Create log entry with proper vector clock and title
                    LogEntry entry(next_entry_id++, op_type, vc, 
                                 op_response.updated_task_id,
                                 task.get_title(),
                                 task.get_description(),
                                 task.get_created_by(),
                                 task.get_column(), 
                                 task.get_client_id());
                    
                    state_machine.append_to_log(entry);
                    
                    if (replication_manager) {
                        replication_manager->replicate_entry(entry);
                    }
                    
                    std::cout << "Created task " << op_response.updated_task_id << " for client " << client_id << "\n";
                }
                
                // Send response with task ID
                stub.SendOperationResponse(op_response);
                continue; // Skip the default SendSuccess
            }
            
            case OpType::UPDATE_TASK: {
                // Get or increment vector clock for this client
                VectorClock vc(client_id);
                {
                    std::lock_guard<std::mutex> clock_lock(clock_mutex);
                    auto clock_it = client_clocks.find(client_id);
                    if (clock_it != client_clocks.end()) {
                        vc = clock_it->second;
                        vc.increment();
                        clock_it->second = vc;
                    } else {
                        client_clocks.insert({client_id, vc});
                    }
                }
                
                // Use conflict detection version (now includes title)
                op_response = task_manager.update_task_with_conflict_detection(
                    task.get_task_id(),
                    task.get_title(),
                    task.get_description(),
                    vc
                );
                success = op_response.success;
                
                if (success && !op_response.rejected) {
                    LogEntry entry(next_entry_id++, op_type, vc,
                                 task.get_task_id(),
                                 task.get_title(),  // Include title for updates
                                 task.get_description(),
                                 "",  // No created_by for updates
                                 Column::TODO,
                                 task.get_client_id());
                    
                    state_machine.append_to_log(entry);
                    
                    if (replication_manager) {
                        replication_manager->replicate_entry(entry);
                    }
                    
                    if (op_response.conflict) {
                        std::cout << "Updated task " << task.get_task_id() << " (with conflict resolution)\n";
                    } else {
                        std::cout << "Updated task " << task.get_task_id() << "\n";
                    }
                }
                
                // Send detailed response
                stub.SendOperationResponse(op_response);
                continue; // Skip the default SendSuccess
            }
            
            case OpType::MOVE_TASK: {
                // Get or increment vector clock for this client
                VectorClock vc(client_id);
                {
                    std::lock_guard<std::mutex> clock_lock(clock_mutex);
                    auto clock_it = client_clocks.find(client_id);
                    if (clock_it != client_clocks.end()) {
                        vc = clock_it->second;
                        vc.increment();
                        clock_it->second = vc;
                    } else {
                        client_clocks.insert({client_id, vc});
                    }
                }
                
                // Use conflict detection version
                op_response = task_manager.move_task_with_conflict_detection(
                    task.get_task_id(),
                    task.get_column(),
                    vc
                );
                success = op_response.success;
                
                if (success && !op_response.rejected) {
                    LogEntry entry(next_entry_id++, op_type, vc,
                                 task.get_task_id(),
                                 "",  // No title for moves
                                 "",
                                 "",  // No created_by for moves
                                 task.get_column(),
                                 task.get_client_id());
                    
                    state_machine.append_to_log(entry);
                    
                    if (replication_manager) {
                        replication_manager->replicate_entry(entry);
                    }
                    
                    if (op_response.conflict) {
                        std::cout << "Moved task " << task.get_task_id() 
                                  << " to column " << static_cast<int>(task.get_column()) 
                                  << " (with conflict resolution)\n";
                    } else {
                        std::cout << "Moved task " << task.get_task_id() 
                                  << " to column " << static_cast<int>(task.get_column()) << "\n";
                    }
                }
                
                // Send detailed response
                stub.SendOperationResponse(op_response);
                continue; // Skip the default SendSuccess
            }
            
            case OpType::DELETE_TASK: {
                // Increment vector clock for delete operation
                VectorClock vc(client_id);
                {
                    std::lock_guard<std::mutex> clock_lock(clock_mutex);
                    auto clock_it = client_clocks.find(client_id);
                    if (clock_it != client_clocks.end()) {
                        vc = clock_it->second;
                        vc.increment();
                        clock_it->second = vc;
                    } else {
                        client_clocks.insert({client_id, vc});
                    }
                }
                
                success = task_manager.delete_task(task.get_task_id());
                
                if (success) {
                    LogEntry entry(next_entry_id++, op_type, vc,
                                 task.get_task_id(),
                                 "",  // No title for deletes
                                 "",
                                 "",  // No created_by for deletes
                                 Column::TODO,
                                 task.get_client_id());
                    
                    state_machine.append_to_log(entry);
                    
                    if (replication_manager) {
                        replication_manager->replicate_entry(entry);
                    }
                    
                    std::cout << "Deleted task " << task.get_task_id() << "\n";
                }
                break;
            }
            
            case OpType::GET_BOARD: {
                // Return all tasks
                std::vector<Task> all_tasks = task_manager.get_all_tasks();
                std::cout << "GET_BOARD request - returning " << all_tasks.size() << " tasks\n";
                
                if (!stub.SendTaskList(all_tasks)) {
                    std::cerr << "Failed to send task list\n";
                }
                continue; // Skip the SendSuccess call
            }
            
            case OpType::HEARTBEAT_PING:
            case OpType::HEARTBEAT_ACK:
            case OpType::MASTER_REJOIN:
            case OpType::STATE_TRANSFER_RESPONSE:
            case OpType::DEMOTE_ACK:
                // Control messages not expected from gateway clients
                std::cerr << "Unexpected control message received\n";
                break;
            
            default:
                std::cerr << "Unknown operation type\n";
                break;
        }
        
        // Send success response to client
        stub.SendSuccess(success);
    }
    
    std::cout << "Client " << client_id << " disconnected\n";
    delete client_socket;
}


int main(int argc, char* argv[]) {
    if (argc != 3 && argc != 5) {
        std::cerr << "Usage: ./master [port] [node_id]\n";
        std::cerr << "   Or: ./master [port] [node_id] [backup_ip] [backup_port]\n";
        return 1;
    }
    
    int port = std::stoi(argv[1]);
    int node_id = std::stoi(argv[2]);
    
    std::cout << "Starting master node " << node_id << " on port " << port << "\n";
    
    // Set up replication if backup specified
    std::string backup_ip;
    int backup_port = 0;
    
    if (argc == 5) {
        backup_ip = argv[3];
        backup_port = std::stoi(argv[4]);
        
        // Try to rejoin from backup (in case backup is promoted after our crash)
        bool rejoined = TryRejoinFromBackup(backup_ip, backup_port);
        
        if (rejoined) {
            std::cout << "Recovered state from promoted backup\n";
        }
        
        std::cout << "Replication target: " << backup_ip << ":" << backup_port << "\n";
        
        // Now set up replication manager to connect to backup
        replication_manager = new ReplicationManager(node_id);
        replication_manager->add_backup(backup_ip, backup_port);
        
        // Start heartbeat monitoring (5 second interval)
        replication_manager->start_heartbeat();
    } else {
        std::cout << "Running without replication (no backup specified)\n";
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
    
    std::cout << "Master listening on port " << port << "...\n";
    
    int client_counter = 0;
    
    // Accept connections
    while (server_running) {
        Socket* client = server_socket.Accept();
        
        if (client && client->IsValid()) {
            int client_id = client_counter++;
            std::thread(HandleClient, client, client_id).detach();
        }
    }
    
    // Cleanup
    if (replication_manager) {
        delete replication_manager;
    }
    
    std::cout << "Server shutdown complete\n";
    return 0;
}
