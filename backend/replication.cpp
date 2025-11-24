#include "replication.h"
#include <iostream>

ReplicationManager::ReplicationManager(int id) : factory_id(id) {
    // Suppress unused warning - factory_id reserved for future use
    (void)factory_id;
}

ReplicationManager::~ReplicationManager() {
    std::cout << "Closing replication connections..." << std::endl;
    for (ClientStub* stub : backup_stubs) {
        if (stub) {
            stub->Close();
            delete stub;
        }
    }
    std::cout << "Replication manager cleaned up" << std::endl;
}

void ReplicationManager::add_backup(const std::string& ip, int port) {
    ClientStub* stub = new ClientStub();
    
    // Try to connect
    if (stub->Init(ip, port)) {
        std::cout << "Connected to backup at " << ip << ":" << port << "\n";
        backup_stubs.push_back(stub);
        backup_connected.push_back(true);
    } else {
        std::cerr << "Failed to connect to backup at " << ip << ":" << port << "\n";
        delete stub;
        backup_stubs.push_back(nullptr);
        backup_connected.push_back(false);
    }
}

void ReplicationManager::connect_to_backups() {
    for (size_t i = 0; i < backup_stubs.size(); i++) {
        if (backup_stubs[i] && !backup_connected[i]) {
            // Try to reconnect
            backup_connected[i] = true;
        }
    }
}

bool ReplicationManager::replicate_entry(const LogEntry& entry) {
    bool any_success = false;
    
    for (size_t i = 0; i < backup_stubs.size(); i++) {
        if (!backup_connected[i] || !backup_stubs[i]) {
            continue;
        }
        
        try {
            // Send log entry to backup
            if (!backup_stubs[i]->SendLogEntry(entry)) {
                std::cerr << "Failed to send to backup " << i << "\n";
                backup_connected[i] = false;
                continue;
            }
            
            // Wait for acknowledgment
            if (!backup_stubs[i]->ReceiveSuccess()) {
                std::cerr << "Backup " << i << " failed to ack\n";
                backup_connected[i] = false;
                continue;
            }
            
            any_success = true;
        } catch (...) {
            backup_connected[i] = false;
        }
    }
    
    return any_success || backup_stubs.empty();
}

bool ReplicationManager::has_backups() const {
    for (bool connected : backup_connected) {
        if (connected) return true;
    }
    return false;
}
