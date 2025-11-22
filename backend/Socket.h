#ifndef SOCKET_H
#define SOCKET_H

#include <string>

// Wrapper for TCP socket operations
class Socket {
private:
    int sock_fd;
    
public:
    Socket();
    ~Socket();
    
    // Client functions
    bool Connect(const std::string& ip, int port);
    
    // Server functions  
    bool Bind(int port);
    bool Listen();
    Socket* Accept();
    
    // Common functions
    bool Send(const void* buffer, size_t size);
    bool Receive(void* buffer, size_t size);
    void Close();
    
    int GetFD() const { return sock_fd; }
    bool IsValid() const { return sock_fd >= 0; }
};

#endif
