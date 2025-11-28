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


bool ClientStub::SendHeartbeat() {
    // Send HEARTBEAT_PING opcode
    int op_type_int = static_cast<int>(OpType::HEARTBEAT_PING);
    int net_op_type = htonl(op_type_int);
    return socket->Send(&net_op_type, sizeof(int));
}

bool ClientStub::ReceiveHeartbeatAck() {
    // Receive simple success response (1 = ack received)
    int result;
    if (!socket->Receive(&result, sizeof(int))) {
        return false;
    }
    return ntohl(result) == 1;
}

// State transfer methods for master rejoin
bool ClientStub::SendStateTransferRequest() {
    return SendOpType(OpType::STATE_TRANSFER_REQUEST);
}

bool ClientStub::ReceiveStateTransfer(std::vector<Task>& tasks, std::vector<LogEntry>& log, int& id_counter) {
    // Receive id_counter
    int net_id_counter;
    if (!socket->Receive(&net_id_counter, sizeof(int))) {
        return false;
    }
    id_counter = ntohl(net_id_counter);
    
    // Receive task count
    int net_task_count;
    if (!socket->Receive(&net_task_count, sizeof(int))) {
        return false;
    }
    int task_count = ntohl(net_task_count);
    
    tasks.clear();
    
    // Receive each task
    for (int i = 0; i < task_count; i++) {
        Task task = ReceiveTask();
        tasks.push_back(task);
    }
    
    // Receive log count
    int net_log_count;
    if (!socket->Receive(&net_log_count, sizeof(int))) {
        return false;
    }
    int log_count = ntohl(net_log_count);
    
    log.clear();
    
    // Receive each log entry
    for (int i = 0; i < log_count; i++) {
        // Receive size
        int net_size;
        if (!socket->Receive(&net_size, sizeof(int))) {
            return false;
        }
        int size = ntohl(net_size);
        
        // Receive data
        char* buffer = new char[size];
        if (!socket->Receive(buffer, size)) {
            delete[] buffer;
            return false;
        }
        
        LogEntry entry(-1, OpType::CREATE_TASK, VectorClock(0), -1, "", "", "", Column::TODO, 0);
        entry.Unmarshal(buffer);
        delete[] buffer;
        log.push_back(entry);
    }
    
    return true;
}

bool ClientStub::SendStateTransfer(const std::vector<Task>& tasks, const std::vector<LogEntry>& log, int id_counter) {
    // Send id_counter
    int net_id_counter = htonl(id_counter);
    if (!socket->Send(&net_id_counter, sizeof(int))) {
        return false;
    }
    
    // Send task count and tasks
    int task_count = tasks.size();
    int net_task_count = htonl(task_count);
    if (!socket->Send(&net_task_count, sizeof(int))) {
        return false;
    }
    
    for (const Task& task : tasks) {
        if (!SendTask(task)) {
            return false;
        }
    }
    
    // Send log count and entries
    int log_count = log.size();
    int net_log_count = htonl(log_count);
    if (!socket->Send(&net_log_count, sizeof(int))) {
        return false;
    }
    
    for (const LogEntry& entry : log) {
        if (!SendLogEntry(entry)) {
            return false;
        }
    }
    
    return true;
}
