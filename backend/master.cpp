#include <iostream>
#include <thread>
#include <vector>
#include <csignal>
#include "Socket.h"
#include "ServerStub.h"
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

void SignalHandler(int) {
    std::cout << "\nShutting down server...\n";
    server_running = false;
    // Close server socket to unblock accept()
    if (global_server_socket) {
        global_server_socket->Close();
    }
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
        
        // Receive task data
        Task task = stub.ReceiveTask();
        bool success = false;
        
        // Process based on operation type
        switch (op_type) {
            case OpType::CREATE_TASK: {
                success = task_manager.create_task(
                    task.get_description(),
                    task.get_client_id()
                );
                
                if (success) {
                    // Create log entry and replicate
                    VectorClock vc(0);
                    LogEntry entry(next_entry_id++, op_type, vc, 
                                 task_manager.get_task_count() - 1,
                                 task.get_description(), 
                                 task.get_column(), 
                                 task.get_client_id());
                    
                    state_machine.append_to_log(entry);
                    
                    if (replication_manager) {
                        replication_manager->replicate_entry(entry);
                    }
                    
                    std::cout << "Created task for client " << client_id << "\n";
                }
                break;
            }
            
            case OpType::UPDATE_TASK: {
                success = task_manager.update_task(
                    task.get_task_id(),
                    task.get_description()
                );
                
                if (success) {
                    VectorClock vc(0);
                    LogEntry entry(next_entry_id++, op_type, vc,
                                 task.get_task_id(),
                                 task.get_description(),
                                 Column::TODO,
                                 task.get_client_id());
                    
                    state_machine.append_to_log(entry);
                    
                    if (replication_manager) {
                        replication_manager->replicate_entry(entry);
                    }
                    
                    std::cout << "Updated task " << task.get_task_id() << "\n";
                }
                break;
            }
            
            case OpType::MOVE_TASK: {
                success = task_manager.move_task(
                    task.get_task_id(),
                    task.get_column()
                );
                
                if (success) {
                    VectorClock vc(0);
                    LogEntry entry(next_entry_id++, op_type, vc,
                                 task.get_task_id(),
                                 "",
                                 task.get_column(),
                                 task.get_client_id());
                    
                    state_machine.append_to_log(entry);
                    
                    if (replication_manager) {
                        replication_manager->replicate_entry(entry);
                    }
                    
                    std::cout << "Moved task " << task.get_task_id() 
                              << " to column " << static_cast<int>(task.get_column()) << "\n";
                }
                break;
            }
            
            case OpType::DELETE_TASK: {
                success = task_manager.delete_task(task.get_task_id());
                
                if (success) {
                    VectorClock vc(0);
                    LogEntry entry(next_entry_id++, op_type, vc,
                                 task.get_task_id(),
                                 "",
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
    // Parse: ./master [port] [node_id] [backup_ip] [backup_port]
    // Or simple mode: ./master [port] [node_id]
    if (argc != 3 && argc != 5) {
        std::cerr << "Usage: ./master [port] [node_id]\n";
        std::cerr << "   Or: ./master [port] [node_id] [backup_ip] [backup_port]\n";
        return 1;
    }
    
    int port = std::stoi(argv[1]);
    int node_id = std::stoi(argv[2]);
    
    std::cout << "Starting master node " << node_id << " on port " << port << "\n";
    
    // Set up replication if backup specified
    if (argc == 5) {
        std::string backup_ip = argv[3];
        int backup_port = std::stoi(argv[4]);
        
        std::cout << "Replication enabled to backup: " << backup_ip << ":" << backup_port << "\n";
        
        replication_manager = new ReplicationManager(node_id);
        replication_manager->add_backup(backup_ip, backup_port);
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
