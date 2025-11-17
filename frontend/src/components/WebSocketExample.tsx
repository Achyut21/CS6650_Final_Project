import { useEffect } from 'react';
import { useWebSocket } from '@/hooks/useWebSocket';
import { WebSocketEvent } from '@/types/websocket';

/**
 * Example component demonstrating WebSocket hook usage
 */
export function WebSocketExample() {
  const { status, subscribe, unsubscribe } = useWebSocket();

  useEffect(() => {
    // Handler for task created events
    const handleTaskCreated = (data: unknown) => {
      console.log('Task created:', data);
    };

    // Handler for task updated events
    const handleTaskUpdated = (data: unknown) => {
      console.log('Task updated:', data);
    };

    // Subscribe to events
    subscribe(WebSocketEvent.TASK_CREATED, handleTaskCreated);
    subscribe(WebSocketEvent.TASK_UPDATED, handleTaskUpdated);

    // Cleanup subscriptions
    return () => {
      unsubscribe(WebSocketEvent.TASK_CREATED, handleTaskCreated);
      unsubscribe(WebSocketEvent.TASK_UPDATED, handleTaskUpdated);
    };
  }, [subscribe, unsubscribe]);

  return (
    <div className="p-4">
      <div className="flex items-center gap-2">
        <div 
          className={`w-3 h-3 rounded-full ${
            status === 'connected' ? 'bg-green-500' : 
            status === 'reconnecting' ? 'bg-yellow-500' : 
            'bg-red-500'
          }`}
        />
        <span style={{ color: '#EEEEEE' }}>
          WebSocket: {status}
        </span>
      </div>
    </div>
  );
}
