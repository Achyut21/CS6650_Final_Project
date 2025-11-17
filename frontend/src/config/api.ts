/**
 * API base URL from environment variable
 */
export const API_BASE_URL = import.meta.env.VITE_API_URL || 'http://localhost:8080';

/**
 * API endpoints
 */
export const API_ENDPOINTS = {
  BOARDS: '/api/boards',
  TASKS: '/api/tasks',
} as const;
