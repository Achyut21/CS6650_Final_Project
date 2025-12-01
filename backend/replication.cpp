#include "replication.h"
#include <iostream>

ReplicationManager::ReplicationManager(int id) : factory_id(id), heartbeat_running(false) {
    // Suppress unused warning - factory_id reserved for future use
    (void)factory_id;
}

ReplicationManager::~ReplicationManager() {
    stop_heartbeat();
    
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
    // Store IP and port for reconnection
    backup_ips.push_back(ip);
    backup_ports.push_back(port);
    
    ClientStub* stub = new ClientStub();
    
    // Try to connect
    if (stub->Init(ip, port)) {
        std::cout << "Connected to backup at " << ip << ":" << port << "\n";
        
        // Send REPLICATION_INIT handshake to identify as master
        if (!stub->SendOpType(OpType::REPLICATION_INIT)) {
            std::cerr << "Failed to send REPLICATION_INIT to backup\n";
            delete stub;
            backup_stubs.push_back(nullptr);
            backup_connected.push_back(false);
            return;
        }
        
        // Wait for acknowledgment
        if (!stub->ReceiveSuccess()) {
            std::cerr << "Backup rejected REPLICATION_INIT (may be promoted)\n";
            delete stub;
            backup_stubs.push_back(nullptr);
            backup_connected.push_back(false);
            return;
        }
        
        std::cout << "Replication handshake successful with backup\n";
        backup_stubs.push_back(stub);
        backup_connected.push_back(true);
    } else {
        std::cerr << "Failed to connect to backup at " << ip << ":" << port << " (will retry)\n";
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
            // Send operation type first (so backup can distinguish from heartbeat)
            if (!backup_stubs[i]->SendOpType(entry.get_op_type())) {
                std::cerr << "Failed to send optype to backup " << i << "\n";
                backup_connected[i] = false;
                continue;
            }
            
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

// Try to reconnect to a disconnected backup
bool ReplicationManager::try_reconnect(size_t index) {
    if (index >= backup_ips.size()) return false;
    
    const std::string& ip = backup_ips[index];
    int port = backup_ports[index];
    
    // Clean up old stub if exists
    if (backup_stubs[index]) {
        backup_stubs[index]->Close();
        delete backup_stubs[index];
        backup_stubs[index] = nullptr;
    }
    
    ClientStub* stub = new ClientStub();
    
    if (!stub->Init(ip, port)) {
        delete stub;
        return false;
    }
    
    // Send REPLICATION_INIT handshake
    if (!stub->SendOpType(OpType::REPLICATION_INIT)) {
        delete stub;
        return false;
    }
    
    // Wait for acknowledgment
    if (!stub->ReceiveSuccess()) {
        delete stub;
        return false;
    }
    
    backup_stubs[index] = stub;
    backup_connected[index] = true;
    std::cout << "[RECONNECT] Successfully reconnected to backup at " << ip << ":" << port << "\n";
    return true;
}

// Send heartbeat to all backups (active probing)
void ReplicationManager::send_heartbeat() {
    for (size_t i = 0; i < backup_stubs.size(); i++) {
        // Try to reconnect disconnected backups
        if (!backup_connected[i] || !backup_stubs[i]) {
            if (try_reconnect(i)) {
                std::cout << "[HEARTBEAT] Backup " << i << " reconnected\n";
            }
            continue;
        }
        
        try {
            // Send HEARTBEAT_PING
            if (!backup_stubs[i]->SendHeartbeat()) {
                std::cout << "[HEARTBEAT] Failed to send ping to backup " << i << " - disconnected\n";
                backup_connected[i] = false;
                continue;
            }
            
            // Receive HEARTBEAT_ACK
            if (!backup_stubs[i]->ReceiveHeartbeatAck()) {
                std::cout << "[HEARTBEAT] No ack from backup " << i << " - disconnected\n";
                backup_connected[i] = false;
                continue;
            }
            
            // Heartbeat successful
        } catch (...) {
            std::cout << "[HEARTBEAT] Exception for backup " << i << " - disconnected\n";
            backup_connected[i] = false;
        }
    }
    
    // Log status
    int connected_count = 0;
    for (size_t i = 0; i < backup_connected.size(); i++) {
        if (backup_connected[i]) {
            connected_count++;
        }
    }
    
    if (connected_count > 0) {
        std::cout << "[HEARTBEAT] " << connected_count << "/" 
                  << backup_connected.size() << " backups alive\n";
    } else if (backup_connected.size() > 0) {
        std::cout << "[HEARTBEAT] WARNING: All backups disconnected!\n";
    }
}

// Heartbeat worker thread - monitors every 5 seconds
void ReplicationManager::heartbeat_worker() {
    std::cout << "[HEARTBEAT] Monitoring started (interval: 5 seconds)\n";
    
    while (heartbeat_running) {
        // Sleep for 5 seconds in small chunks to allow quick shutdown
        for (int i = 0; i < 50 && heartbeat_running; i++) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        
        if (!heartbeat_running) break;
        
        send_heartbeat();
    }
    
    std::cout << "[HEARTBEAT] Monitoring stopped\n";
}

// Start heartbeat monitoring
void ReplicationManager::start_heartbeat() {
    if (!heartbeat_running) {
        heartbeat_running = true;
        heartbeat_thread = std::thread(&ReplicationManager::heartbeat_worker, this);
        std::cout << "Heartbeat monitoring started\n";
    }
}

// Stop heartbeat monitoring
void ReplicationManager::stop_heartbeat() {
    if (heartbeat_running) {
        heartbeat_running = false;
        if (heartbeat_thread.joinable()) {
            heartbeat_thread.join();
        }
        std::cout << "Heartbeat monitoring stopped\n";
    }
}
