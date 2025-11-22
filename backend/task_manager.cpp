#include <string>
#include <stdexcept>
#include <algorithm>
#include "task_manager.h"

TaskManager::TaskManager()
{
    id_counter = 0;
}

// Create a task with a new vector clock,
// column = TODO and add pointer to the clock to clock vector, return true.
bool TaskManager::create_task(std::string description, int client_id)
{
    std::lock_guard<std::mutex> lock(task_lock);
    VectorClock new_clock(id_counter);
    tasks.emplace(id_counter, Task(id_counter, description, Column::TODO, client_id));

    clocks.push_back(&tasks[id_counter].get_clock());

    id_counter++;
    return true;
}

// Look for id in task map, it it doesn't exist return false
// If it exists set the new description and return true
bool TaskManager::update_task(int task_id, const std::string &description)
{
    std::lock_guard<std::mutex> lock(task_lock);
    auto it = tasks.find(task_id);
    if (it == tasks.end())
    {
        return false; // Task not found
    }

    it->second.set_description(description);
    return true;
}

// Looks for id in task map, if it doesn't exist return false
// If it exists, if column is already set - return true, else set column to new column and return true.
bool TaskManager::move_task(int task_id, Column column)
{
    std::lock_guard<std::mutex> lock(task_lock);

    auto it = tasks.find(task_id);
    if (it == tasks.end())
    {
        return false; // Task not found
    }

    if (it->second.get_column() == column)
    {
        return true;
    }

    it->second.set_column(column);
    return true;
}

// Looks for given id in task map. If it doesn't exist return false.
// If it exists, remove pointer to vectorclock and then erase the task.
bool TaskManager::delete_task(int task_id)
{
    std::lock_guard<std::mutex> lock(task_lock);

    auto task_it = tasks.find(task_id);
    if (task_it == tasks.end())
    {
        return false;
    }

    VectorClock *clock_to_remove = &task_it->second.get_clock();
    tasks.erase(task_it);
    auto it = std::find(clocks.begin(), clocks.end(), clock_to_remove);
    if (it != clocks.end())
    {
        clocks.erase(it);
    }

    return true;
}

// Search for task by id in task map. If not found, throw error - if found return Task
Task TaskManager::get_task(int id)
{
    std::lock_guard<std::mutex> lock(task_lock);

    auto it = tasks.find(id);
    if (it == tasks.end())
    {
        throw std::runtime_error("Task not found");
    }

    return it->second;
}

size_t TaskManager::get_task_count() const
{
    return tasks.size();
}