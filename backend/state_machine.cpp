#
#include <stdexcept>
#include "state_machine.h"
#include <vector>

StateMachine::StateMachine(int id, TaskManager *tm) : task_manager(tm), server_id(id)
{
    primary_id = -1;
    last_index = -1;
    committed_index = -1;
}

void StateMachine::append_to_log(const LogEntry &entry)
{
    std::lock_guard<std::mutex> lock(state_mutex);
    smr_log.push_back(entry);
    last_index = smr_log.size() - 1;
}

void StateMachine::apply_entry(int index)
{
    std::lock_guard<std::mutex> lock(state_mutex);

    if (index < 0 || index >= smr_log.size())
    {
        throw std::out_of_range("Invalid log index");
    }

    const LogEntry &entry = smr_log[index];

    switch (entry.get_op_type())
    {
    case OpType::CREATE_TASK:
        task_manager->create_task(entry.get_description(), entry.get_client_id());
        break;
    case OpType::UPDATE_TASK:
        task_manager->update_task(entry.get_task_id(), entry.get_description());
        break;
    case OpType::MOVE_TASK:
        task_manager->move_task(entry.get_task_id(), entry.get_column());
        break;
    case OpType::DELETE_TASK:
        task_manager->delete_task(entry.get_task_id());
        break;
    }
    committed_index++;
}

void StateMachine::commit_up_to(int index)
{
    std::lock_guard<std::mutex> lock(state_mutex);

    if (index >= smr_log.size())
    {
        throw std::out_of_range("Invalid Log Index");
    }

    if (index < committed_index)
    {
        return; // Already committed
    }
    for (int i = committed_index + 1; i <= index; i++)
    {
        const LogEntry &entry = smr_log[i];
        switch (entry.get_op_type())
        {
        case OpType::CREATE_TASK:
            task_manager->create_task(entry.get_description(), entry.get_client_id());
            break;
        case OpType::UPDATE_TASK:
            task_manager->update_task(entry.get_task_id(), entry.get_description());
            break;
        case OpType::MOVE_TASK:
            task_manager->move_task(entry.get_task_id(), entry.get_column());
            break;
        case OpType::DELETE_TASK:
            task_manager->delete_task(entry.get_task_id());
            break;
        }
    }
    committed_index = index;
}

LogEntry StateMachine::get_log_entry(int index) const
{
    std::lock_guard<std::mutex> lock(state_mutex);

    if (index < 0 || index >= smr_log.size())
    {
        throw std::out_of_range("Invalid Log Index");
    }

    const LogEntry &entry = smr_log[index];
    return entry;
}

std::vector<LogEntry> StateMachine::get_log() const
{
    std::lock_guard<std::mutex> lock(state_mutex);
    return smr_log;
}

std::vector<LogEntry> StateMachine::get_log_after(int index) const
{
    std::vector<LogEntry> result;

    for (size_t i = index + 1; i < smr_log.size(); i++)
    {
        result.push_back(smr_log[i]);
    }

    return result;
}

void StateMachine::replay_log()
{
    std::lock_guard<std::mutex> lock(state_mutex);

    delete task_manager;
    task_manager = new TaskManager();

    for (int i = 0; i < smr_log.size(); i++)
    {
        this->apply_entry(i);
    }
}

void StateMachine::replay_from(int start_index)
{
    std::lock_guard<std::mutex> lock(state_mutex);

    for (int i = start_index; i < smr_log.size(); i++)
    {
        this->apply_entry(i);
    }

    committed_index = smr_log.size() - 1;
}

int StateMachine::get_last_index()
{
    return last_index;
}

int StateMachine::get_committed_index()
{
    return committed_index;
}

int StateMachine::get_primary_id()
{
    return primary_id;
}

int StateMachine::get_server_id()
{
    return server_id;
}

void StateMachine::set_primary_id(int id)
{
    this->primary_id = id;
}

bool StateMachine::is_primary()
{
    return primary_id == server_id;
}
