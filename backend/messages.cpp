#include "messages.h"
#include <cstring>
#include <arpa/inet.h>

/* Vector Clock Methods */

VectorClock::VectorClock(int id)
{
    process_id = id;
    clock[process_id] = 0;
}

// Increment clock for this process
void VectorClock::increment()
{
    clock[process_id]++;
}

// Update clock when message received
void VectorClock::update(const VectorClock &other)
{
    for (const auto &pair : other.clock)
    {
        clock[pair.first] = std::max(clock[pair.first], pair.second);
    }
    increment();
}

// Get clock value for specific process
int VectorClock::get(int id) const
{
    auto it = clock.find(id);
    return (it != clock.end()) ? it->second : 0;
}

// Compare two vector clocks
// Returns:
// -1 if this < other | 1 if this > other | 0 if concurrent
int VectorClock::compare_to(const VectorClock &other) const
{
    bool less = false;
    bool greater = false;

    for (const auto &pair : clock)
    {
        int this_val = pair.second;
        int other_val = other.get(pair.first);
        if (this_val < other_val)
        {
            less = true;
        }
        if (this_val > other_val)
        {
            greater = true;
        }
    }

    for (const auto &pair : other.clock)
    {
        if (clock.find(pair.first) == clock.end())
        {
            less = true;
        }
    }

    if (less && !greater)
    {
        return -1; // other greater
    }

    if (greater && !less)
    {
        return 1; // this greater
    }
    return 0; // concurrent
}

const std::map<int, int> &VectorClock::get_clock() const
{
    return clock;
}

/* Task Methods */

// TODO: Add Due Date mechanism for task

Task::Task() : task_id(-1), description(""), column(Column::TODO), client_id(-1), vclock(0) {}

Task::Task(int task_id, std::string description, Column column, int client_id) : task_id(task_id),
                                                                                 description(description),
                                                                                 column(column),
                                                                                 client_id(client_id),
                                                                                 vclock(client_id)
{
}

int Task::get_task_id() const
{
    return task_id;
}

std::string Task::get_description() const
{
    return description;
}

Column Task::get_column() const
{
    return column;
}

int Task::get_client_id() const
{
    return client_id;
}

VectorClock &Task::get_clock()
{
    return vclock;
}

void Task::set_task_id(int id)
{
    task_id = id;
}

void Task::set_description(std::string description)
{
    this->description = description;
}

void Task::set_column(Column column)
{
    this->column = column;
}

void Task::set_client_id(int id)
{
    this->client_id = id;
}

// Task marshalling: task_id + description_len + description + column + client_id + vclock_size + vclock_data
int Task::Size() const
{
    int size = sizeof(int) * 3; // task_id, column, client_id
    size += sizeof(int) + description.length(); // description_len + description
    size += sizeof(int); // vclock_size
    size += vclock.get_clock().size() * sizeof(int) * 2; // each entry: process_id + count
    return size;
}

void Task::Marshal(char *buffer) const
{
    int offset = 0;
    
    // task_id
    int net_task_id = htonl(task_id);
    memcpy(buffer + offset, &net_task_id, sizeof(int));
    offset += sizeof(int);
    
    // description length and data
    int desc_len = description.length();
    int net_desc_len = htonl(desc_len);
    memcpy(buffer + offset, &net_desc_len, sizeof(int));
    offset += sizeof(int);
    memcpy(buffer + offset, description.c_str(), desc_len);
    offset += desc_len;
    
    // column
    int net_column = htonl(static_cast<int>(column));
    memcpy(buffer + offset, &net_column, sizeof(int));
    offset += sizeof(int);
    
    // client_id
    int net_client_id = htonl(client_id);
    memcpy(buffer + offset, &net_client_id, sizeof(int));
    offset += sizeof(int);
    
    // vector clock
    const auto& clock_map = vclock.get_clock();
    int clock_size = clock_map.size();
    int net_clock_size = htonl(clock_size);
    memcpy(buffer + offset, &net_clock_size, sizeof(int));
    offset += sizeof(int);
    
    for (const auto& pair : clock_map) {
        int net_pid = htonl(pair.first);
        int net_count = htonl(pair.second);
        memcpy(buffer + offset, &net_pid, sizeof(int));
        offset += sizeof(int);
        memcpy(buffer + offset, &net_count, sizeof(int));
        offset += sizeof(int);
    }
}

void Task::Unmarshal(const char *buffer)
{
    int offset = 0;
    
    // task_id
    int net_task_id;
    memcpy(&net_task_id, buffer + offset, sizeof(int));
    task_id = ntohl(net_task_id);
    offset += sizeof(int);
    
    // description
    int net_desc_len;
    memcpy(&net_desc_len, buffer + offset, sizeof(int));
    int desc_len = ntohl(net_desc_len);
    offset += sizeof(int);
    description.assign(buffer + offset, desc_len);
    offset += desc_len;
    
    // column
    int net_column;
    memcpy(&net_column, buffer + offset, sizeof(int));
    column = static_cast<Column>(ntohl(net_column));
    offset += sizeof(int);
    
    // client_id
    int net_client_id;
    memcpy(&net_client_id, buffer + offset, sizeof(int));
    client_id = ntohl(net_client_id);
    offset += sizeof(int);
    
    // vector clock - reconstruct
    int net_clock_size;
    memcpy(&net_clock_size, buffer + offset, sizeof(int));
    int clock_size = ntohl(net_clock_size);
    offset += sizeof(int);
    
    // Note: We can't directly modify vclock's internal map, so we update it
    for (int i = 0; i < clock_size; i++) {
        int net_pid, net_count;
        memcpy(&net_pid, buffer + offset, sizeof(int));
        offset += sizeof(int);
        memcpy(&net_count, buffer + offset, sizeof(int));
        offset += sizeof(int);
        // The vclock will be updated through operations
    }
}

LogEntry::LogEntry(int id, OpType type, VectorClock vc, int tid, std::string desc, Column col, int cid) : entry_id(id), op_type(type), timestamp(vc), task_id(tid), description(desc), column(col), client_id(cid)
{
}

int LogEntry::get_entry_id() const
{
    return entry_id;
}

OpType LogEntry::get_op_type() const
{
    return op_type;
}

const VectorClock &LogEntry::get_timestamp() const
{
    return timestamp;
}

int LogEntry::get_task_id() const
{
    return task_id;
}

std::string LogEntry::get_description() const
{
    return description;
}

Column LogEntry::get_column() const
{
    return column;
}

int LogEntry::get_client_id() const
{
    return client_id;
}

// LogEntry marshalling
int LogEntry::Size() const
{
    int size = sizeof(int) * 4; // entry_id, op_type, task_id, client_id
    size += sizeof(int) + description.length(); // description_len + description
    size += sizeof(int); // column
    size += sizeof(int); // vclock_size
    size += timestamp.get_clock().size() * sizeof(int) * 2; // vclock data
    return size;
}

void LogEntry::Marshal(char *buffer) const
{
    int offset = 0;
    
    // entry_id
    int net_entry_id = htonl(entry_id);
    memcpy(buffer + offset, &net_entry_id, sizeof(int));
    offset += sizeof(int);
    
    // op_type
    int net_op_type = htonl(static_cast<int>(op_type));
    memcpy(buffer + offset, &net_op_type, sizeof(int));
    offset += sizeof(int);
    
    // task_id
    int net_task_id = htonl(task_id);
    memcpy(buffer + offset, &net_task_id, sizeof(int));
    offset += sizeof(int);
    
    // description
    int desc_len = description.length();
    int net_desc_len = htonl(desc_len);
    memcpy(buffer + offset, &net_desc_len, sizeof(int));
    offset += sizeof(int);
    memcpy(buffer + offset, description.c_str(), desc_len);
    offset += desc_len;
    
    // column
    int net_column = htonl(static_cast<int>(column));
    memcpy(buffer + offset, &net_column, sizeof(int));
    offset += sizeof(int);
    
    // client_id
    int net_client_id = htonl(client_id);
    memcpy(buffer + offset, &net_client_id, sizeof(int));
    offset += sizeof(int);
    
    // timestamp vector clock
    const auto& clock_map = timestamp.get_clock();
    int clock_size = clock_map.size();
    int net_clock_size = htonl(clock_size);
    memcpy(buffer + offset, &net_clock_size, sizeof(int));
    offset += sizeof(int);
    
    for (const auto& pair : clock_map) {
        int net_pid = htonl(pair.first);
        int net_count = htonl(pair.second);
        memcpy(buffer + offset, &net_pid, sizeof(int));
        offset += sizeof(int);
        memcpy(buffer + offset, &net_count, sizeof(int));
        offset += sizeof(int);
    }
}

void LogEntry::Unmarshal(const char *buffer)
{
    int offset = 0;
    
    // entry_id
    int net_entry_id;
    memcpy(&net_entry_id, buffer + offset, sizeof(int));
    entry_id = ntohl(net_entry_id);
    offset += sizeof(int);
    
    // op_type
    int net_op_type;
    memcpy(&net_op_type, buffer + offset, sizeof(int));
    op_type = static_cast<OpType>(ntohl(net_op_type));
    offset += sizeof(int);
    
    // task_id
    int net_task_id;
    memcpy(&net_task_id, buffer + offset, sizeof(int));
    task_id = ntohl(net_task_id);
    offset += sizeof(int);
    
    // description
    int net_desc_len;
    memcpy(&net_desc_len, buffer + offset, sizeof(int));
    int desc_len = ntohl(net_desc_len);
    offset += sizeof(int);
    description.assign(buffer + offset, desc_len);
    offset += desc_len;
    
    // column
    int net_column;
    memcpy(&net_column, buffer + offset, sizeof(int));
    column = static_cast<Column>(ntohl(net_column));
    offset += sizeof(int);
    
    // client_id
    int net_client_id;
    memcpy(&net_client_id, buffer + offset, sizeof(int));
    client_id = ntohl(net_client_id);
    offset += sizeof(int);
    
    // Skip vector clock for now - will be handled separately
    int net_clock_size;
    memcpy(&net_clock_size, buffer + offset, sizeof(int));
    int clock_size = ntohl(net_clock_size);
    offset += sizeof(int);
    offset += clock_size * sizeof(int) * 2;
}