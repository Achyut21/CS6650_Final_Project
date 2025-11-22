#ifndef SERVER_STUB_H
#define SERVER_STUB_H

#include "Socket.h"
#include "messages.h"

// Server stub for receiving task operations
class ServerStub {
private:
    Socket* socket;
    
public:
    ServerStub();
    ~ServerStub();
    
    bool Init(Socket* client_socket);
    
    // Receive operation type
    OpType ReceiveOpType();
    
    // Receive task operation data
    Task ReceiveTask();
    LogEntry ReceiveLogEntry();
    
    // Send responses
    bool SendTask(const Task& task);
    bool SendSuccess(bool success);
    
    void Close();
};

#endif
