/**
 * Column values representing task status
 */
export const Column = {
  TODO: 0,
  IN_PROGRESS: 1,
  DONE: 2,
} as const;

/**
 * Column type derived from Column object
 */
export type Column = typeof Column[keyof typeof Column];

/**
 * Task data structure matching backend format
 */
export interface Task {
  task_id: number;
  board_id: string;
  title: string;
  description: string;
  column: Column;
  created_by: string;
  vector_clock: Record<string, number>;
  created_at: number;
  updated_at: number;
}

/**
 * Board containing all tasks
 */
export interface Board {
  board_id: string;
  tasks: Task[];
}

/**
 * Payload for creating a new task
 */
export interface CreateTaskPayload {
  board_id: string;
  title: string;
  description: string;
  column: Column;
  created_by: string;
}

/**
 * Payload for updating an existing task
 */
export interface UpdateTaskPayload {
  title?: string;
  description?: string;
  column?: Column;
}
