#include <string>
#include <stdexcept>
#include <algorithm>
#include <iostream>
#include <chrono>
#include "task_manager.h"

TaskManager::TaskManager()
{
    id_counter = 0;
}

// Create a task with all fields, including timestamps
bool TaskManager::create_task(std::string title, std::string description, 
                               std::string board_id, std::string created_by, int client_id)
{
    std::lock_guard<std::mutex> lock(task_lock);
    
    // Get current timestamp (milliseconds since epoch)
    long long now = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();
    
    // Create task with all fields
    Task new_task(id_counter, title, description, board_id, created_by, Column::TODO, client_id);
    new_task.set_updated_at(now);
    
    tasks.emplace(id_counter, new_task);
    clocks.push_back(&tasks[id_counter].get_clock());

    id_counter++;
    return true;
}

// Update task with vector clock conflict detection
// Returns true if update applied, false if rejected due to causality
bool TaskManager::update_task(int task_id, const std::string &description, const VectorClock &new_clock)
{
    std::lock_guard<std::mutex> lock(task_lock);
    auto it = tasks.find(task_id);
    if (it == tasks.end())
    {
        return false; // Task not found
    }

    // Compare vector clocks to detect conflicts
    int comparison = it->second.get_clock().compare_to(new_clock);
    
    if (comparison == 0) {
        // Concurrent updates - use last-write-wins (already applied)
        std::cout << "[CONFLICT] Concurrent update detected for task " << task_id 
                  << " - applying last-write-wins\n";
        it->second.set_description(description);
        it->second.get_clock().update(new_clock);
        it->second.set_updated_at(std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count());
        return true;
    } else if (comparison < 0) {
        // New update is causally newer - apply it
        it->second.set_description(description);
        it->second.get_clock().update(new_clock);
        it->second.set_updated_at(std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count());
        return true;
    } else {
        // Old update - reject it
        std::cout << "[CONFLICT] Rejecting old update for task " << task_id 
                  << " (outdated by vector clock)\n";
        return false;
    }
}

// Move task with vector clock conflict detection
bool TaskManager::move_task(int task_id, Column column, const VectorClock &new_clock)
{
    std::lock_guard<std::mutex> lock(task_lock);

    auto it = tasks.find(task_id);
    if (it == tasks.end())
    {
        return false; // Task not found
    }

    if (it->second.get_column() == column)
    {
        return true; // Already in target column
    }

    // Compare vector clocks
    int comparison = it->second.get_clock().compare_to(new_clock);
    
    if (comparison == 0) {
        // Concurrent moves, here apply last-write-wins
        std::cout << "[CONFLICT] Concurrent move detected for task " << task_id 
                  << " - applying move to column " << static_cast<int>(column) << "\n";
        it->second.set_column(column);
        it->second.get_clock().update(new_clock);
        it->second.set_updated_at(std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count());
        return true;
    } else if (comparison < 0) {
        // New move is causally newer
        it->second.set_column(column);
        it->second.get_clock().update(new_clock);
        it->second.set_updated_at(std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count());
        return true;
    } else {
        // Old move - reject
        std::cout << "[CONFLICT] Rejecting old move for task " << task_id 
                  << " (outdated by vector clock)\n";
        return false;
    }
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

std::vector<Task> TaskManager::get_all_tasks()
{
    std::lock_guard<std::mutex> lock(task_lock);
    std::vector<Task> all_tasks;
    
    for (const auto& pair : tasks) {
        all_tasks.push_back(pair.second);
    }
    
    return all_tasks;
}
// Update task with conflict detection and returns detailed response
OperationResponse TaskManager::update_task_with_conflict_detection(int task_id, const std::string &description, const VectorClock &new_clock)
{
    OperationResponse response;
    response.updated_task_id = task_id;
    
    std::lock_guard<std::mutex> lock(task_lock);
    auto it = tasks.find(task_id);
    if (it == tasks.end())
    {
        response.success = false;
        return response;
    }

    int comparison = it->second.get_clock().compare_to(new_clock);
    
    if (comparison == 0) {
        // Concurrent updates and apply with conflict flag
        std::cout << "[CONFLICT] Concurrent update detected for task " << task_id 
                  << " - applying last-write-wins\n";
        it->second.set_description(description);
        it->second.get_clock().update(new_clock);
        it->second.set_updated_at(std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count());
        response.success = true;
        response.conflict = true;
        response.rejected = false;
        return response;
    } else if (comparison < 0) {
        // New update is causally newer - apply normally
        it->second.set_description(description);
        it->second.get_clock().update(new_clock);
        it->second.set_updated_at(std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count());
        response.success = true;
        response.conflict = false;
        response.rejected = false;
        return response;
    } else {
        // Old update - reject it
        std::cout << "[CONFLICT] Rejecting old update for task " << task_id 
                  << " (outdated by vector clock)\n";
        response.success = false;
        response.conflict = false;
        response.rejected = true;
        return response;
    }
}

// Move task with conflict detection and returns detailed response
OperationResponse TaskManager::move_task_with_conflict_detection(int task_id, Column column, const VectorClock &new_clock)
{
    OperationResponse response;
    response.updated_task_id = task_id;
    
    std::lock_guard<std::mutex> lock(task_lock);
    auto it = tasks.find(task_id);
    if (it == tasks.end())
    {
        response.success = false;
        return response;
    }

    if (it->second.get_column() == column)
    {
        response.success = true;
        return response;
    }

    int comparison = it->second.get_clock().compare_to(new_clock);
    
    if (comparison == 0) {
        // Concurrent moves and apply with conflict flag
        std::cout << "[CONFLICT] Concurrent move detected for task " << task_id 
                  << " - applying move to column " << static_cast<int>(column) << "\n";
        it->second.set_column(column);
        it->second.get_clock().update(new_clock);
        it->second.set_updated_at(std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count());
        response.success = true;
        response.conflict = true;
        response.rejected = false;
        return response;
    } else if (comparison < 0) {
        // New move is causally newer
        it->second.set_column(column);
        it->second.get_clock().update(new_clock);
        it->second.set_updated_at(std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count());
        response.success = true;
        response.conflict = false;
        response.rejected = false;
        return response;
    } else {
        // Old move - reject
        std::cout << "[CONFLICT] Rejecting old move for task " << task_id 
                  << " (outdated by vector clock)\n";
        response.success = false;
        response.conflict = false;
        response.rejected = true;
        return response;
    }
}

// Backward compatible create_task for tests
bool TaskManager::create_task(std::string description, int client_id)
{
    return create_task("Task", description, "board-1", "user", client_id);
}
