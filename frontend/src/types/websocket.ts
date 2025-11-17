import type { Task } from './task';

/**
 * WebSocket event types from backend
 */
export const WebSocketEvent = {
  TASK_CREATED: 'TASK_CREATED',
  TASK_UPDATED: 'TASK_UPDATED',
  TASK_MOVED: 'TASK_MOVED',
  TASK_DELETED: 'TASK_DELETED',
} as const;

export type WebSocketEvent = typeof WebSocketEvent[keyof typeof WebSocketEvent];

/**
 * Base WebSocket message structure
 */
export interface WebSocketMessage {
  event: WebSocketEvent;
  data: Task | { task_id: number };
}

/**
 * Task created message
 */
export interface TaskCreatedMessage {
  event: typeof WebSocketEvent.TASK_CREATED;
  data: Task;
}

/**
 * Task updated message
 */
export interface TaskUpdatedMessage {
  event: typeof WebSocketEvent.TASK_UPDATED;
  data: Task;
}

/**
 * Task moved message
 */
export interface TaskMovedMessage {
  event: typeof WebSocketEvent.TASK_MOVED;
  data: Task;
}

/**
 * Task deleted message
 */
export interface TaskDeletedMessage {
  event: typeof WebSocketEvent.TASK_DELETED;
  data: { task_id: number };
}

/**
 * Union type of all WebSocket messages
 */
export type WebSocketMessageType =
  | TaskCreatedMessage
  | TaskUpdatedMessage
  | TaskMovedMessage
  | TaskDeletedMessage;
