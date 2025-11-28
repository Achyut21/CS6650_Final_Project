#include "messages.h"
#include <cstring>
#include <arpa/inet.h>
#include <chrono>

#ifndef htonll
static inline uint64_t htonll(uint64_t value) {
    static const int num = 1;
    if (*(char *)&num == 1) {
        uint32_t high = htonl((uint32_t)(value >> 32));
        uint32_t low = htonl((uint32_t)(value & 0xFFFFFFFFULL));
        return ((uint64_t)low << 32) | high;
    }
    return value;
}
#endif

#ifndef ntohll
static inline uint64_t ntohll(uint64_t value) {
    return htonll(value); // Same operation
}
#endif

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

// Set clock value for specific process (used during unmarshalling)
void VectorClock::set(int id, int value)
{
    clock[id] = value;
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

Task::Task() : task_id(-1), title(""), description(""), board_id("board-1"), 
               created_by(""), column(Column::TODO), client_id(-1), 
               created_at(0), updated_at(0), vclock(0) {}

Task::Task(int task_id, std::string title, std::string description,
           std::string board_id, std::string created_by, Column column, int client_id) 
    : task_id(task_id),
      title(title),
      description(description),
      board_id(board_id),
      created_by(created_by),
      column(column),
      client_id(client_id),
      vclock(client_id)
{
    // Set timestamps to current time
    long long now = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();
    created_at = now;
    updated_at = now;
}

int Task::get_task_id() const
{
    return task_id;
}

std::string Task::get_title() const
{
    return title;
}

std::string Task::get_description() const
{
    return description;
}

std::string Task::get_board_id() const
{
    return board_id;
}

std::string Task::get_created_by() const
{
    return created_by;
}

Column Task::get_column() const
{
    return column;
}

int Task::get_client_id() const
{
    return client_id;
}

long long Task::get_created_at() const
{
    return created_at;
}

long long Task::get_updated_at() const
{
    return updated_at;
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

void Task::set_title(std::string title)
{
    this->title = title;
}

void Task::set_board_id(std::string board_id)
{
    this->board_id = board_id;
}

void Task::set_created_by(std::string created_by)
{
    this->created_by = created_by;
}

void Task::set_updated_at(long long timestamp)
{
    this->updated_at = timestamp;
}

// Task marshalling: task_id + description_len + description + column + client_id + vclock_size + vclock_data
// Task marshalling with all fields
int Task::Size() const
{
    int size = sizeof(int) * 3; // task_id, column, client_id
    size += sizeof(long long) * 2; // created_at, updated_at (8 bytes each)
    size += sizeof(int) + title.length(); // title_len + title
    size += sizeof(int) + description.length(); // description_len + description
    size += sizeof(int) + board_id.length(); // board_id_len + board_id
    size += sizeof(int) + created_by.length(); // created_by_len + created_by
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
    
    // title
    int title_len = title.length();
    int net_title_len = htonl(title_len);
    memcpy(buffer + offset, &net_title_len, sizeof(int));
    offset += sizeof(int);
    memcpy(buffer + offset, title.c_str(), title_len);
    offset += title_len;
    
    // description
    int desc_len = description.length();
    int net_desc_len = htonl(desc_len);
    memcpy(buffer + offset, &net_desc_len, sizeof(int));
    offset += sizeof(int);
    memcpy(buffer + offset, description.c_str(), desc_len);
    offset += desc_len;
    
    // board_id
    int board_id_len = board_id.length();
    int net_board_id_len = htonl(board_id_len);
    memcpy(buffer + offset, &net_board_id_len, sizeof(int));
    offset += sizeof(int);
    memcpy(buffer + offset, board_id.c_str(), board_id_len);
    offset += board_id_len;
    
    // created_by
    int created_by_len = created_by.length();
    int net_created_by_len = htonl(created_by_len);
    memcpy(buffer + offset, &net_created_by_len, sizeof(int));
    offset += sizeof(int);
    memcpy(buffer + offset, created_by.c_str(), created_by_len);
    offset += created_by_len;
    
    // column
    int net_column = htonl(static_cast<int>(column));
    memcpy(buffer + offset, &net_column, sizeof(int));
    offset += sizeof(int);
    
    // client_id
    int net_client_id = htonl(client_id);
    memcpy(buffer + offset, &net_client_id, sizeof(int));
    offset += sizeof(int);
    
    // created_at (8 bytes)
    long long net_created_at = htonll(created_at);
    memcpy(buffer + offset, &net_created_at, sizeof(long long));
    offset += sizeof(long long);
    
    // updated_at (8 bytes)
    long long net_updated_at = htonll(updated_at);
    memcpy(buffer + offset, &net_updated_at, sizeof(long long));
    offset += sizeof(long long);
    
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
    
    // title
    int net_title_len;
    memcpy(&net_title_len, buffer + offset, sizeof(int));
    int title_len = ntohl(net_title_len);
    offset += sizeof(int);
    title.assign(buffer + offset, title_len);
    offset += title_len;
    
    // description
    int net_desc_len;
    memcpy(&net_desc_len, buffer + offset, sizeof(int));
    int desc_len = ntohl(net_desc_len);
    offset += sizeof(int);
    description.assign(buffer + offset, desc_len);
    offset += desc_len;
    
    // board_id
    int net_board_id_len;
    memcpy(&net_board_id_len, buffer + offset, sizeof(int));
    int board_id_len = ntohl(net_board_id_len);
    offset += sizeof(int);
    board_id.assign(buffer + offset, board_id_len);
    offset += board_id_len;
    
    // created_by
    int net_created_by_len;
    memcpy(&net_created_by_len, buffer + offset, sizeof(int));
    int created_by_len = ntohl(net_created_by_len);
    offset += sizeof(int);
    created_by.assign(buffer + offset, created_by_len);
    offset += created_by_len;
    
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
    
    // created_at (8 bytes)
    long long net_created_at;
    memcpy(&net_created_at, buffer + offset, sizeof(long long));
    created_at = ntohll(net_created_at);
    offset += sizeof(long long);
    
    // updated_at (8 bytes)
    long long net_updated_at;
    memcpy(&net_updated_at, buffer + offset, sizeof(long long));
    updated_at = ntohll(net_updated_at);
    offset += sizeof(long long);
    
    // vector clock - reconstruct using set() method
    int net_clock_size;
    memcpy(&net_clock_size, buffer + offset, sizeof(int));
    int clock_size = ntohl(net_clock_size);
    offset += sizeof(int);
    
    for (int i = 0; i < clock_size; i++) {
        int net_pid, net_count;
        memcpy(&net_pid, buffer + offset, sizeof(int));
        int pid = ntohl(net_pid);
        offset += sizeof(int);
        memcpy(&net_count, buffer + offset, sizeof(int));
        int count = ntohl(net_count);
        offset += sizeof(int);
        vclock.set(pid, count);  // Actually restore the vector clock entry
    }
}

LogEntry::LogEntry(int id, OpType type, VectorClock vc, int tid, std::string title, std::string desc, std::string created_by, Column col, int cid) : entry_id(id), op_type(type), timestamp(vc), task_id(tid), title(title), description(desc), created_by(created_by), column(col), client_id(cid)
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

std::string LogEntry::get_title() const
{
    return title;
}

std::string LogEntry::get_description() const
{
    return description;
}

std::string LogEntry::get_created_by() const
{
    return created_by;
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
    size += sizeof(int) + title.length(); // title_len + title
    size += sizeof(int) + description.length(); // description_len + description
    size += sizeof(int) + created_by.length(); // created_by_len + created_by
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
    
    // title
    int title_len = title.length();
    int net_title_len = htonl(title_len);
    memcpy(buffer + offset, &net_title_len, sizeof(int));
    offset += sizeof(int);
    memcpy(buffer + offset, title.c_str(), title_len);
    offset += title_len;
    
    // description
    int desc_len = description.length();
    int net_desc_len = htonl(desc_len);
    memcpy(buffer + offset, &net_desc_len, sizeof(int));
    offset += sizeof(int);
    memcpy(buffer + offset, description.c_str(), desc_len);
    offset += desc_len;
    
    // created_by
    int created_by_len = created_by.length();
    int net_created_by_len = htonl(created_by_len);
    memcpy(buffer + offset, &net_created_by_len, sizeof(int));
    offset += sizeof(int);
    memcpy(buffer + offset, created_by.c_str(), created_by_len);
    offset += created_by_len;
    
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
    
    // title
    int net_title_len;
    memcpy(&net_title_len, buffer + offset, sizeof(int));
    int title_len = ntohl(net_title_len);
    offset += sizeof(int);
    title.assign(buffer + offset, title_len);
    offset += title_len;
    
    // description
    int net_desc_len;
    memcpy(&net_desc_len, buffer + offset, sizeof(int));
    int desc_len = ntohl(net_desc_len);
    offset += sizeof(int);
    description.assign(buffer + offset, desc_len);
    offset += desc_len;
    
    // created_by
    int net_created_by_len;
    memcpy(&net_created_by_len, buffer + offset, sizeof(int));
    int created_by_len = ntohl(net_created_by_len);
    offset += sizeof(int);
    created_by.assign(buffer + offset, created_by_len);
    offset += created_by_len;
    
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