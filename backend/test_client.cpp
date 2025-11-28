#include <iostream>
#include <unistd.h>
#include <cstring>
#include <arpa/inet.h>
#include "ClientStub.h"
#include "messages.h"
#include "Socket.h"

// Helper to receive OperationResponse (4 ints: success, conflict, rejected, task_id)
struct OperationResponseData {
    bool success;
    bool conflict;
    bool rejected;
    int task_id;
};

bool ReceiveOperationResponse(Socket* socket, OperationResponseData& response) {
    int buffer[4];
    if (!socket->Receive(buffer, sizeof(buffer))) {
        return false;
    }
    response.success = (ntohl(buffer[0]) == 1);
    response.conflict = (ntohl(buffer[1]) == 1);
    response.rejected = (ntohl(buffer[2]) == 1);
    response.task_id = ntohl(buffer[3]);
    return true;
}

int main() {
    std::cout << "Testing connection to master at 127.0.0.1:12345...\n";
    
    // Check if master is reachable
    std::cout << "Attempting connection...\n";
    
    // Use raw Socket for more control over response handling
    Socket socket;
    if (!socket.Connect("127.0.0.1", 12345)) {
        std::cerr << " Failed to connect to master\n";
        std::cerr << " Is master running? (./master 12345 0)\n";
        std::cerr << " Is port 12345 accessible?\n";
        return 1;
    }
    
    std::cout << "Connected to master!\n";
    
    // Test CREATE_TASK
    std::cout << "Sending CREATE_TASK operation...\n";
    
    // Send op type
    int op_type = htonl(static_cast<int>(OpType::CREATE_TASK));
    if (!socket.Send(&op_type, sizeof(int))) {
        std::cerr << "Failed to send op type\n";
        return 1;
    }
    
    // Task(task_id, title, description, board_id, created_by, column, client_id)
    Task task(0, "Test Task", "Test Task from Client", "board-1", "test_user", Column::TODO, 1);
    
    // Marshal and send task
    int size = task.Size();
    char* buffer = new char[size];
    task.Marshal(buffer);
    
    int net_size = htonl(size);
    if (!socket.Send(&net_size, sizeof(int))) {
        std::cerr << "Failed to send task size\n";
        delete[] buffer;
        return 1;
    }
    
    if (!socket.Send(buffer, size)) {
        std::cerr << "Failed to send task data\n";
        delete[] buffer;
        return 1;
    }
    delete[] buffer;
    
    std::cout << "Waiting for response...\n";
    
    // Receive OperationResponse (16 bytes: 4 ints)
    OperationResponseData response;
    if (!ReceiveOperationResponse(&socket, response)) {
        std::cerr << "Failed to receive response\n";
        return 1;
    }
    
    if (response.success) {
        std::cout << " Task created successfully!\n";
        std::cout << " Task ID: " << response.task_id << "\n";
        if (response.conflict) {
            std::cout << " (Conflict was detected and resolved)\n";
        }
    } else {
        std::cout << " Task creation failed\n";
        if (response.rejected) {
            std::cout << " (Operation was rejected due to outdated vector clock)\n";
        }
    }
    
    socket.Close();
    return 0;
}
