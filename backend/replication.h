#ifndef __REPLICATION_H__
#define __REPLICATION_H__

#include <vector>
#include <thread>
#include <atomic>
#include <chrono>
#include "Socket.h"
#include "ClientStub.h"
#include "messages.h"

// Manages replication to backup nodes
class ReplicationManager {
private:
    int factory_id;
    std::vector<ClientStub*> backup_stubs;
    std::vector<bool> backup_connected;
    std::vector<std::string> backup_ips;
    std::vector<int> backup_ports;
    std::atomic<bool> heartbeat_running;
    std::thread heartbeat_thread;
    
    // Heartbeat worker function
    void heartbeat_worker();
    
    // Try to reconnect to a disconnected backup
    bool try_reconnect(size_t index);

public:
    ReplicationManager(int id);
    ~ReplicationManager();
    
    // Add backup peer (id, ip, port)
    void add_backup(const std::string& ip, int port);
    
    // Connect to all backups
    void connect_to_backups();
    
    // Replicate log entry to all backups
    bool replicate_entry(const LogEntry& entry);
    
    // Send heartbeat to all backups
    void send_heartbeat();
    
    // Start heartbeat monitoring
    void start_heartbeat();
    
    // Stop heartbeat monitoring
    void stop_heartbeat();
    
    // Check if any backups are connected
    bool has_backups() const;
};

#endif
