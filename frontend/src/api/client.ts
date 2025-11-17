import { API_BASE_URL, API_ENDPOINTS } from '@/config/api';
import type { Board, Task, CreateTaskPayload, UpdateTaskPayload } from '@/types/task';

/**
 * Get all tasks for a board
 */
export async function getTasks(boardId: string): Promise<Board> {
  const response = await fetch(`${API_BASE_URL}${API_ENDPOINTS.BOARDS}/${boardId}`);
  
  if (!response.ok) {
    throw new Error(`Failed to fetch tasks: ${response.statusText}`);
  }
  
  return response.json();
}

/**
 * Create a new task
 */
export async function createTask(payload: CreateTaskPayload): Promise<Task> {
  const response = await fetch(`${API_BASE_URL}${API_ENDPOINTS.TASKS}`, {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify(payload),
  });
  
  if (!response.ok) {
    throw new Error(`Failed to create task: ${response.statusText}`);
  }
  
  return response.json();
}

/**
 * Update an existing task
 */
export async function updateTask(taskId: number, payload: UpdateTaskPayload): Promise<Task> {
  const response = await fetch(`${API_BASE_URL}${API_ENDPOINTS.TASKS}/${taskId}`, {
    method: 'PATCH',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify(payload),
  });
  
  if (!response.ok) {
    throw new Error(`Failed to update task: ${response.statusText}`);
  }
  
  return response.json();
}

/**
 * Delete a task
 */
export async function deleteTask(taskId: number): Promise<void> {
  const response = await fetch(`${API_BASE_URL}${API_ENDPOINTS.TASKS}/${taskId}`, {
    method: 'DELETE',
  });
  
  if (!response.ok) {
    throw new Error(`Failed to delete task: ${response.statusText}`);
  }
}
