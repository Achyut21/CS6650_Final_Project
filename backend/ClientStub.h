#ifndef CLIENT_STUB_H
#define CLIENT_STUB_H

#include "Socket.h"
#include "messages.h"
#include <string>

// Client stub for sending task operations
class ClientStub {
private:
    Socket* socket;
    
public:
    ClientStub();
    ~ClientStub();
    
    bool Init(const std::string& ip, int port);
    
    // Send operation type
    bool SendOpType(OpType op_type);
    
    // Send task operation data
    bool SendTask(const Task& task);
    bool SendLogEntry(const LogEntry& entry);
    
    // Receive responses
    Task ReceiveTask();
    bool ReceiveSuccess();
    
    void Close();
};

#endif
