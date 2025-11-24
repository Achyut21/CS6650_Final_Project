#include <iostream>
#include <unistd.h>
#include "ClientStub.h"
#include "messages.h"

int main() {
    std::cout << "Testing connection to master at 127.0.0.1:12345...\n";
    
    // Check if master is reachable
    std::cout << "Attempting connection...\n";
    
    ClientStub client;
    if (!client.Init("127.0.0.1", 12345)) {
        std::cerr << " Failed to connect to master\n";
        std::cerr << " Is master running? (./master 12345 0)\n";
        std::cerr << " Is port 12345 accessible?\n";
        return 1;
    }
    
    std::cout << "Connected to master!\n";
    
    // Test CREATE_TASK
    std::cout << "Sending CREATE_TASK operation...\n";
    if (!client.SendOpType(OpType::CREATE_TASK)) {
        std::cerr << "Failed to send op type\n";
        return 1;
    }
    
    Task task(0, "Test Task from Client", Column::TODO, 1);
    if (!client.SendTask(task)) {
        std::cerr << "Failed to send task\n";
        return 1;
    }
    
    std::cout << "Waiting for response...\n";
    bool success = client.ReceiveSuccess();
    
    if (success) {
        std::cout << " Task created successfully!\n";
    } else {
        std::cout << " Task creation failed\n";
    }
    
    client.Close();
    return 0;
}
