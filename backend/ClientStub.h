#ifndef CLIENT_STUB_H
#define CLIENT_STUB_H

#include <vector>
#include <string>
#include "Socket.h"
#include "messages.h"

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
    
    // Heartbeat operations
    bool SendHeartbeat();
    bool ReceiveHeartbeatAck();
    
    // Receive responses
    Task ReceiveTask();
    bool ReceiveSuccess();
    
    // State transfer methods for master rejoin
    bool SendStateTransferRequest();
    bool ReceiveStateTransfer(std::vector<Task>& tasks, std::vector<LogEntry>& log, int& id_counter);
    bool SendStateTransfer(const std::vector<Task>& tasks, const std::vector<LogEntry>& log, int id_counter);
    
    void Close();
};

#endif
