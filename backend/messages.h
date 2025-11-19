#ifndef __MESSAGES_H__
#define __MESSAGES_H__

#include <string>
#include <map>

enum class OpType
{
    CREATE_TASK,
    UPDATE_TASK,
    MOVE_TASK,
    DELETE_TASK
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
    int get(int id) const;
    int compare_to(const VectorClock &other) const;
    const std::map<int, int> &get_clock() const;
};

class Task
{
private:
    int task_id;
    std::string description;
    Column column;
    int client_id;
    VectorClock vclock;

public:
    Task();
    Task(int task_id, std::string description, Column column, int client_id);

    int get_task_id();
    std::string get_description();
    Column get_column();
    int get_client_id();
    VectorClock &get_clock();

    void set_task_id(int id);
    void set_description(std::string description);
    void set_column(Column column);
    void set_client_id(int id);
};

class LogEntry
{
private:
    int entry_id;
    OpType op_type;
    VectorClock timestamp;

    // Operation parameters
    int task_id;             // For update/move/delete
    std::string description; // For create/update
    Column column;           // For move
    int client_id;           // For create

public:
    LogEntry(int id, OpType type, VectorClock vc,
             int tid, std::string desc, Column col, int cid);

    // Getters
    int get_entry_id() const;
    OpType get_op_type() const;
    const VectorClock &get_timestamp() const;

    int get_task_id() const;
    std::string get_description() const;
    Column get_column() const;
    int get_client_id() const;
};

#endif