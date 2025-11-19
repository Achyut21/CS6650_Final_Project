#include "messages.h"

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

int Task::get_task_id()
{
    return task_id;
}

std::string Task::get_description()
{
    return description;
}

Column Task::get_column()
{
    return column;
}

int Task::get_client_id()
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

LogEntry::LogEntry(int id, OperationType type, VectorClock vc, int tid, std::string desc, Column col, int cid); : entry_id(id), op_type(type), timestamp(vc), task_id(tid), description(desc), column(col), client_id(cid)
{
}

int LogEntry::get_entry_id()
{
    return entry_id;
}

OperationType LogEntry::get_op_type()
{
    return op_type;
}

const VectorClock &LogEntry::get_timestamp() const
{
    return timestamp;
}

int LogEntry::get_task_id()
{
    return task_id;
}

std::string LogEntry::get_description()
{
    return description;
}

Column LogEntry::get_column()
{
    return column;
}

LogEntry::get_client_id()
{
    return client_id;
}