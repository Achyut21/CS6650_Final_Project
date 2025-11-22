#include "Socket.h"
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <iostream>

Socket::Socket() : sock_fd(-1) {}

Socket::~Socket() {
    if (sock_fd >= 0) {
        close(sock_fd);
    }
}

bool Socket::Connect(const std::string& ip, int port) {
    sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (sock_fd < 0) return false;
    
    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    
    if (inet_pton(AF_INET, ip.c_str(), &server_addr.sin_addr) <= 0) {
        return false;
    }
    
    if (connect(sock_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        return false;
    }
    
    return true;
}

bool Socket::Bind(int port) {
    sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (sock_fd < 0) return false;
    
    // Allow socket reuse
    int opt = 1;
    if (setsockopt(sock_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        return false;
    }
    
    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);
    
    if (bind(sock_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        return false;
    }
    
    return true;
}

bool Socket::Listen() {
    return listen(sock_fd, 10) >= 0;
}

Socket* Socket::Accept() {
    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);
    
    int client_fd = accept(sock_fd, (struct sockaddr*)&client_addr, &client_len);
    if (client_fd < 0) return nullptr;
    
    Socket* client_socket = new Socket();
    client_socket->sock_fd = client_fd;
    return client_socket;
}

bool Socket::Send(const void* buffer, size_t size) {
    size_t total_sent = 0;
    const char* data = (const char*)buffer;
    
    while (total_sent < size) {
        ssize_t sent = send(sock_fd, data + total_sent, size - total_sent, 0);
        if (sent <= 0) return false;
        total_sent += sent;
    }
    
    return true;
}

bool Socket::Receive(void* buffer, size_t size) {
    size_t total_received = 0;
    char* data = (char*)buffer;
    
    while (total_received < size) {
        ssize_t received = recv(sock_fd, data + total_received, size - total_received, 0);
        if (received <= 0) return false;
        total_received += received;
    }
    
    return true;
}

void Socket::Close() {
    if (sock_fd >= 0) {
        close(sock_fd);
        sock_fd = -1;
    }
}
