#ifndef __REPLICATION_H__
#define __REPLICATION_H__

#include <vector>
#include "Socket.h"
#include "ClientStub.h"
#include "messages.h"

// Manages replication to backup nodes
class ReplicationManager {
private:
    int factory_id;
    std::vector<ClientStub*> backup_stubs;
    std::vector<bool> backup_connected;

public:
    ReplicationManager(int id);
    ~ReplicationManager();
    
    // Add backup peer (id, ip, port)
    void add_backup(const std::string& ip, int port);
    
    // Connect to all backups
    void connect_to_backups();
    
    // Replicate log entry to all backups
    bool replicate_entry(const LogEntry& entry);
    
    // Check if any backups are connected
    bool has_backups() const;
};

#endif
