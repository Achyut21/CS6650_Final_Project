import { useEffect, useRef, useState } from 'react';
import { io, Socket } from 'socket.io-client';
import { API_BASE_URL } from '@/config/api';

type ConnectionStatus = 'connected' | 'disconnected' | 'reconnecting';
type EventCallback = (data: unknown) => void;

/**
 * Custom hook for managing WebSocket connection with automatic reconnection
 */
export function useWebSocket() {
  const [status, setStatus] = useState<ConnectionStatus>('disconnected');
  const socketRef = useRef<Socket | null>(null);
  const reconnectTimeoutRef = useRef<ReturnType<typeof setTimeout> | null>(null);
  const reconnectAttempts = useRef(0);
  const eventHandlers = useRef<Map<string, Set<EventCallback>>>(new Map());

  useEffect(() => {
    // Calculate exponential backoff delay (1s, 2s, 4s, 8s max)
    const getReconnectDelay = () => {
      return Math.min(1000 * Math.pow(2, reconnectAttempts.current), 8000);
    };

    // Connect to WebSocket server
    const connect = () => {
      if (socketRef.current?.connected) return;

      const socket = io(API_BASE_URL, {
        transports: ['websocket'],
        reconnection: false, // We handle reconnection manually
      });

      socketRef.current = socket;

      socket.on('connect', () => {
        setStatus('connected');
        reconnectAttempts.current = 0;
        console.log('WebSocket connected');
      });

      socket.on('disconnect', () => {
        setStatus('disconnected');
        console.log('WebSocket disconnected');
        scheduleReconnect();
      });

      socket.on('connect_error', () => {
        setStatus('disconnected');
        scheduleReconnect();
      });

      // Forward all events to registered handlers
      socket.onAny((eventName: string, data: unknown) => {
        const handlers = eventHandlers.current.get(eventName);
        if (handlers) {
          handlers.forEach(handler => handler(data));
        }
      });
    };

    // Schedule reconnection with exponential backoff
    const scheduleReconnect = () => {
      if (reconnectTimeoutRef.current) return;

      setStatus('reconnecting');
      const delay = getReconnectDelay();
      
      reconnectTimeoutRef.current = setTimeout(() => {
        reconnectTimeoutRef.current = null;
        reconnectAttempts.current++;
        connect();
      }, delay);
    };

    // Initialize connection
    connect();

    // Cleanup on unmount
    return () => {
      if (reconnectTimeoutRef.current) {
        clearTimeout(reconnectTimeoutRef.current);
      }
      if (socketRef.current) {
        socketRef.current.disconnect();
        socketRef.current = null;
      }
    };
  }, []);

  // Subscribe to a specific event
  const subscribe = (event: string, callback: EventCallback) => {
    if (!eventHandlers.current.has(event)) {
      eventHandlers.current.set(event, new Set());
    }
    eventHandlers.current.get(event)!.add(callback);
  };

  // Unsubscribe from a specific event
  const unsubscribe = (event: string, callback: EventCallback) => {
    const handlers = eventHandlers.current.get(event);
    if (handlers) {
      handlers.delete(callback);
    }
  };

  return { status, subscribe, unsubscribe };
}
