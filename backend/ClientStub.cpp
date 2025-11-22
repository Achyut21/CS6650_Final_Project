#include "ClientStub.h"
#include <cstring>
#include <arpa/inet.h>

ClientStub::ClientStub() : socket(nullptr) {}

ClientStub::~ClientStub() {
    if (socket) {
        delete socket;
    }
}

bool ClientStub::Init(const std::string& ip, int port) {
    socket = new Socket();
    return socket->Connect(ip, port);
}

bool ClientStub::SendOpType(OpType op_type) {
    int op_type_int = static_cast<int>(op_type);
    int net_op_type = htonl(op_type_int);
    return socket->Send(&net_op_type, sizeof(int));
}

bool ClientStub::SendTask(const Task& task) {
    int size = task.Size();
    char* buffer = new char[size];
    task.Marshal(buffer);
    
    // Send size first
    int net_size = htonl(size);
    if (!socket->Send(&net_size, sizeof(int))) {
        delete[] buffer;
        return false;
    }
    
    // Send data
    bool result = socket->Send(buffer, size);
    delete[] buffer;
    return result;
}

bool ClientStub::SendLogEntry(const LogEntry& entry) {
    int size = entry.Size();
    char* buffer = new char[size];
    entry.Marshal(buffer);
    
    // Send size first
    int net_size = htonl(size);
    if (!socket->Send(&net_size, sizeof(int))) {
        delete[] buffer;
        return false;
    }
    
    // Send data
    bool result = socket->Send(buffer, size);
    delete[] buffer;
    return result;
}

Task ClientStub::ReceiveTask() {
    Task task;
    
    // Receive size
    int size;
    if (!socket->Receive(&size, sizeof(int))) {
        return task;
    }
    size = ntohl(size);
    
    // Receive data
    char* buffer = new char[size];
    if (!socket->Receive(buffer, size)) {
        delete[] buffer;
        return task;
    }
    
    task.Unmarshal(buffer);
    delete[] buffer;
    return task;
}

bool ClientStub::ReceiveSuccess() {
    int result;
    if (!socket->Receive(&result, sizeof(int))) {
        return false;
    }
    return ntohl(result) == 1;
}

void ClientStub::Close() {
    if (socket) {
        socket->Close();
        delete socket;
        socket = nullptr;
    }
}

