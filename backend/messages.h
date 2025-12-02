#ifndef __MESSAGES_H__
#define __MESSAGES_H__

#include <string>
#include <map>

enum class OpType
{
    CREATE_TASK,
    UPDATE_TASK,
    MOVE_TASK,
    DELETE_TASK,
    GET_BOARD,
    HEARTBEAT_PING,
    HEARTBEAT_ACK,
    // Master rejoin protocol
    MASTER_REJOIN, // Master announces it's rejoining
    STATE_TRANSFER_REQUEST, // Request full state from promoted backup
    STATE_TRANSFER_RESPONSE, // Backup sends state to master
    DEMOTE_ACK, // Backup acknowledges demotion
    REPLICATION_INIT         // Replication Handshake, Master identifies itself when connecting for replication
};

// Response status for operations
struct OperationResponse {
    bool success;
    bool conflict;           // True if concurrent operation detected
    bool rejected;          // True if operation rejected due to outdated vector clock
    int updated_task_id;    // ID of task that was updated
    
    OperationResponse() : success(false), conflict(false), rejected(false), updated_task_id(-1) {}
};

enum class Column
{
    TODO,
    IN_PROGRESS,
    DONE
};

class VectorClock
{
private:
    std::map<int, int> clock;
    int process_id;

public:
    VectorClock(int id);
    void increment();
    void update(const VectorClock &other);
    void set(int id, int value);  // Set clock value for a process (used during unmarshal)
    void clear();  // Clear all entries (used before unmarshal to remove stale data)
    int get(int id) const;
    int compare_to(const VectorClock &other) const;
    const std::map<int, int> &get_clock() const;
};

class Task
{
private:
    int task_id;
    std::string title;
    std::string description;
    std::string board_id;
    std::string created_by;
    Column column;
    int client_id;
    long long created_at;
    long long updated_at;
    VectorClock vclock;

public:
    Task();
    Task(int task_id, std::string title, std::string description, 
         std::string board_id, std::string created_by, Column column, int client_id);

    int get_task_id() const;
    std::string get_title() const;
    std::string get_description() const;
    std::string get_board_id() const;
    std::string get_created_by() const;
    Column get_column() const;
    int get_client_id() const;
    long long get_created_at() const;
    long long get_updated_at() const;
    VectorClock &get_clock();

    void set_task_id(int id);
    void set_title(std::string title);
    void set_description(std::string description);
    void set_board_id(std::string board_id);
    void set_created_by(std::string created_by);
    void set_column(Column column);
    void set_client_id(int id);
    void set_updated_at(long long timestamp);

    // Marshalling
    int Size() const;
    void Marshal(char *buffer) const;
    void Unmarshal(const char *buffer);
};

class LogEntry
{
private:
    int entry_id;
    OpType op_type;
    VectorClock timestamp;

    // Operation parameters
    int task_id;             // For update/move/delete
    std::string title;       // For create
    std::string description; // For create/update
    std::string created_by;  // For create
    Column column;           // For move/create
    int client_id;           // For create

public:
    LogEntry(int id, OpType type, VectorClock vc,
             int tid, std::string title, std::string desc, std::string created_by, Column col, int cid);

    // Getters
    int get_entry_id() const;
    OpType get_op_type() const;
    const VectorClock &get_timestamp() const;

    int get_task_id() const;
    std::string get_title() const;
    std::string get_description() const;
    std::string get_created_by() const;
    Column get_column() const;
    int get_client_id() const;

    // Marshalling
    int Size() const;
    void Marshal(char *buffer) const;
    void Unmarshal(const char *buffer);
};

#endif