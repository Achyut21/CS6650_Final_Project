#include <iostream>
#include <thread>
#include <chrono>
#include <csignal>
#include "Socket.h"
#include "ServerStub.h"
#include "task_manager.h"
#include "state_machine.h"
#include "messages.h"

// Global variables
TaskManager task_manager;
StateMachine state_machine;
bool server_running = true;
bool is_promoted = false;
int backup_port = 12346;  // Updated from argv in main()
Socket* global_server_socket = nullptr;
std::map<int, VectorClock> client_clocks;  // Track vector clock per client after promotion
std::mutex clock_mutex;

void SignalHandler(int) {
    std::cout << "\nShutting down backup...\n";
    server_running = false;
    // Close server socket to unblock accept()
    if (global_server_socket) {
        global_server_socket->Close();
    }
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
                
                // Send OperationResponse with task ID
                op_response.success = success;
                op_response.conflict = false;
                op_response.rejected = false;
                op_response.updated_task_id = success ? (int)(task_manager.get_task_count() - 1) : -1;
                
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
                        task.get_task_id(), task.get_description(), vc);
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
                // Promoted backup shouldn't receive heartbeat messages
                std::cerr << "Unexpected heartbeat in promoted backup\n";
                break;
                
            default:
                break;
        }
        
        stub.SendSuccess(success);
    }
    
    std::cout << "Client " << client_id << " disconnected\n";
    delete client_socket;
}

// Handle replication from primary
void HandleReplication(Socket* client_socket) {
    ServerStub stub;
    if (!stub.Init(client_socket)) {
        delete client_socket;
        return;
    }
    
    std::cout << "Primary connected for replication\n";
    
    while (true) {
        // First receive operation type
        OpType op_type = stub.ReceiveOpType();
        
        if (static_cast<int>(op_type) == -1) {
            std::cout << "ReceiveOpType failed - Primary disconnected" << std::endl;
            std::cout << "PROMOTING TO MASTER" << std::endl;
            is_promoted = true;
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
        
        // For task operations, receive the log entry
        LogEntry entry = stub.ReceiveLogEntry();
        
        // Check for disconnect (entry_id == -1 indicates error)
        if (entry.get_entry_id() < 0) {
            std::cout << "ReceiveLogEntry failed - Primary disconnected" << std::endl;
            std::cout << "PROMOTING TO MASTER" << std::endl;
            is_promoted = true;
            std::cout << "Backup promoted! Now accepting client connections on port " << backup_port << std::endl;
            std::cout << "Total tasks replicated: " << task_manager.get_task_count() << std::endl;
            std::cout << "State machine log size: " << state_machine.get_log_size() << std::endl;
            std::cout.flush();
            break;
        }
        
        // Append to state machine log
        state_machine.append_to_log(entry);
        
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
                task_manager.update_task(entry.get_task_id(), entry.get_description(), vc);
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
                // Heartbeat operations handled separately, not logged
                break;
        }
        
        // Send acknowledgment
        if (!stub.SendSuccess(true)) {
            std::cout << "Failed to send ack to primary" << std::endl;
            std::cout << "Primary disconnected - PROMOTING TO MASTER" << std::endl;
            is_promoted = true;
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
    
    int client_counter = 0;
    
    // Accept connections (from primary for replication OR from clients after promotion)
    while (server_running) {
        if (is_promoted) {
            std::cout << "[PROMOTED MODE] Waiting for client connections..." << std::endl;
        }
        
        Socket* socket = server_socket.Accept();
        
        if (socket && socket->IsValid()) {
            if (!is_promoted) {
                // Still backup - handle replication
                std::cout << "[BACKUP MODE] Handling replication connection" << std::endl;
                std::thread(HandleReplication, socket).detach();
                // Note: is_promoted may be set to true inside HandleReplication
            } else {
                // Promoted to master - handle clients like master does
                std::cout << "[PROMOTED MODE] Handling client connection" << std::endl;
                int client_id = client_counter++;
                std::thread(HandleClient, socket, client_id).detach();
            }
        } else if (!server_running) {
            // Server shutting down
            break;
        }
    }
    
    std::cout << "Backup shutdown complete\n";
    return 0;
}
