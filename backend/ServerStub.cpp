#include "ServerStub.h"
#include <cstring>
#include <arpa/inet.h>

ServerStub::ServerStub() : socket(nullptr) {}

ServerStub::~ServerStub() {
    // Socket is owned by caller, don't delete
}

bool ServerStub::Init(Socket* client_socket) {
    socket = client_socket;
    return socket != nullptr && socket->IsValid();
}

OpType ServerStub::ReceiveOpType() {
    int op_type_int;
    if (!socket->Receive(&op_type_int, sizeof(int))) {
        return static_cast<OpType>(-1); // Error
    }
    return static_cast<OpType>(ntohl(op_type_int));
}

Task ServerStub::ReceiveTask() {
    Task task;
    
    // First receive size
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

LogEntry ServerStub::ReceiveLogEntry() {
    // Error entry with -1 id to indicate failure
    LogEntry entry(-1, OpType::CREATE_TASK, VectorClock(0), -1, "", Column::TODO, 0);
    
    // First receive size
    int size;
    if (!socket->Receive(&size, sizeof(int))) {
        return entry; // Returns entry with id=-1
    }
    size = ntohl(size);
    
    // Receive data
    char* buffer = new char[size];
    if (!socket->Receive(buffer, size)) {
        delete[] buffer;
        return entry; // Returns entry with id=-1
    }
    
    entry.Unmarshal(buffer);
    delete[] buffer;
    return entry;
}

bool ServerStub::SendTask(const Task& task) {
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

bool ServerStub::SendTaskList(const std::vector<Task>& tasks) {
    // Send count first
    int count = tasks.size();
    int net_count = htonl(count);
    if (!socket->Send(&net_count, sizeof(int))) {
        return false;
    }
    
    // Send each task
    for (const Task& task : tasks) {
        if (!SendTask(task)) {
            return false;
        }
    }
    
    return true;
}

bool ServerStub::SendSuccess(bool success) {
    int result = success ? 1 : 0;
    int net_result = htonl(result);
    return socket->Send(&net_result, sizeof(int));
}

void ServerStub::Close() {
    if (socket) {
        socket->Close();
    }
}


bool ServerStub::SendOperationResponse(const OperationResponse& response) {
    // Send 4 integers: success, conflict, rejected, task_id
    int buffer[4];
    buffer[0] = htonl(response.success ? 1 : 0);
    buffer[1] = htonl(response.conflict ? 1 : 0);
    buffer[2] = htonl(response.rejected ? 1 : 0);
    buffer[3] = htonl(response.updated_task_id);
    
    return socket->Send(buffer, sizeof(buffer));
}
