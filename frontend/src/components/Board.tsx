import { useState, useEffect, useCallback } from 'react';
import { monitorForElements } from '@atlaskit/pragmatic-drag-and-drop/element/adapter';
import type { Task } from '@/types/task';
import { Column as ColumnType } from '@/types/task';
import { WebSocketEvent } from '@/types/websocket';
import { Column } from './Column';
import { TaskModal } from './TaskModal';
import { ConnectionStatus } from './ConnectionStatus';
import { useToast } from '@/hooks/useToast';
import { useWebSocket } from '@/hooks/useWebSocket';
import * as api from '@/api/client';

const BOARD_ID = 'board-1';

// Sample tasks for when backend is unavailable
const SAMPLE_TASKS: Task[] = [
  {
    task_id: 1,
    board_id: 'board-1',
    title: 'Setup project repository',
    description: 'Initialize Git repo and create basic project structure',
    column: ColumnType.DONE,
    created_by: 'Alice',
    vector_clock: { alice: 1 },
    created_at: Date.now() - 86400000,
    updated_at: Date.now() - 86400000,
  },
  {
    task_id: 2,
    board_id: 'board-1',
    title: 'Implement backend API',
    description: 'Create REST endpoints and WebSocket handlers',
    column: ColumnType.IN_PROGRESS,
    created_by: 'Bob',
    vector_clock: { bob: 1 },
    created_at: Date.now() - 43200000,
    updated_at: Date.now() - 43200000,
  },
  {
    task_id: 3,
    board_id: 'board-1',
    title: 'Design UI mockups',
    description: 'Create wireframes for main board and task cards',
    column: ColumnType.TODO,
    created_by: 'Charlie',
    vector_clock: { charlie: 1 },
    created_at: Date.now(),
    updated_at: Date.now(),
  },
];

/**
 * Main kanban board component with drag-and-drop
 */
export function Board() {
  const [tasks, setTasks] = useState<Task[]>([]);
  const [isModalOpen, setIsModalOpen] = useState(false);
  const [editingTask, setEditingTask] = useState<Task | null>(null);
  const [isLoading, setIsLoading] = useState(true);
  const [remoteUpdateIds, setRemoteUpdateIds] = useState<Set<number>>(new Set());
  const { showToast } = useToast();
  const { status: wsStatus, subscribe, unsubscribe } = useWebSocket();

  const loadTasks = useCallback(async () => {
    try {
      const board = await api.getTasks(BOARD_ID);
      setTasks(board.tasks);
    } catch {
      showToast('Backend not available - using sample data', 'info');
      setTasks(SAMPLE_TASKS);
    } finally {
      setIsLoading(false);
    }
  }, [showToast]);

  useEffect(() => {
    loadTasks();
  }, [loadTasks]);

  // Reload tasks on WebSocket reconnection to sync state
  useEffect(() => {
    if (wsStatus === 'connected') {
      console.log('WebSocket reconnected - resyncing board state...');
      loadTasks();
    }
  }, [wsStatus, loadTasks]);

  // Subscribe to WebSocket events for real-time updates
  useEffect(() => {
    const markAsRemoteUpdate = (taskId: number) => {
      setRemoteUpdateIds(prev => new Set(prev).add(taskId));
      setTimeout(() => {
        setRemoteUpdateIds(prev => {
          const next = new Set(prev);
          next.delete(taskId);
          return next;
        });
      }, 600); // Match animation duration
    };

    const handleTaskCreated = (data: unknown) => {
      const { task } = data as { task: Task };
      setTasks(prev => {
        // Check if task already exists (from our own optimistic update)
        if (prev.some(t => t.task_id === task.task_id)) {
          return prev;
        }
        return [...prev, task];
      });
      markAsRemoteUpdate(task.task_id);
      showToast(`New task created by ${task.created_by}`, 'info');
    };

    const handleTaskUpdated = (data: unknown) => {
      const { task } = data as { task: Task };
      setTasks(prev => prev.map(t => t.task_id === task.task_id ? { ...t, ...task } : t));
      markAsRemoteUpdate(task.task_id);
    };

    const handleTaskMoved = (data: unknown) => {
      const { task } = data as { task: Task };
      setTasks(prev => prev.map(t => t.task_id === task.task_id ? { ...t, ...task } : t));
      markAsRemoteUpdate(task.task_id);
    };

    const handleTaskDeleted = (data: unknown) => {
      const taskData = data as { task_id: number };
      setTasks(prev => prev.filter(t => t.task_id !== taskData.task_id));
    };

    subscribe(WebSocketEvent.TASK_CREATED, handleTaskCreated);
    subscribe(WebSocketEvent.TASK_UPDATED, handleTaskUpdated);
    subscribe(WebSocketEvent.TASK_MOVED, handleTaskMoved);
    subscribe(WebSocketEvent.TASK_DELETED, handleTaskDeleted);

    return () => {
      unsubscribe(WebSocketEvent.TASK_CREATED, handleTaskCreated);
      unsubscribe(WebSocketEvent.TASK_UPDATED, handleTaskUpdated);
      unsubscribe(WebSocketEvent.TASK_MOVED, handleTaskMoved);
      unsubscribe(WebSocketEvent.TASK_DELETED, handleTaskDeleted);
    };
  }, [subscribe, unsubscribe, showToast]);

  // Monitor drop events
  useEffect(() => {
    return monitorForElements({
      onDrop: async ({ source, location }) => {
        const destination = location.current.dropTargets[0];
        if (!destination) return;

        const sourceData = source.data;
        const destData = destination.data;

        if (
          sourceData.type === 'task' &&
          destData.type === 'column' &&
          typeof destData.columnId === 'number'
        ) {
          const task = sourceData.task as Task;
          const newColumn = destData.columnId as ColumnType;

          if (task.column === newColumn) return;

          const previousTasks = [...tasks];
          setTasks(tasks.map(t => 
            t.task_id === task.task_id ? { ...t, column: newColumn } : t
          ));

          try {
            await api.updateTask(task.task_id, { column: newColumn });
            showToast('Task moved successfully', 'success');
          } catch {
            setTasks(previousTasks);
            showToast('Failed to move task - backend unavailable', 'error');
          }
        }
      },
    });
  }, [tasks, showToast]);

  const handleEditTask = (task: Task) => {
    setEditingTask(task);
    setIsModalOpen(true);
  };

  const handleCreateTask = () => {
    setEditingTask(null);
    setIsModalOpen(true);
  };

  const handleSaveTask = async (data: { title: string; description: string; column: ColumnType }) => {
    if (editingTask) {
      const previousTasks = [...tasks];
      setTasks(tasks.map(t =>
        t.task_id === editingTask.task_id
          ? { ...t, ...data, updated_at: Date.now() }
          : t
      ));

      try {
        await api.updateTask(editingTask.task_id, data);
        showToast('Task updated successfully', 'success');
      } catch {
        setTasks(previousTasks);
        showToast('Failed to update - backend unavailable', 'error');
      }
    } else {
      const tempId = Date.now();
      const newTask: Task = {
        task_id: tempId,
        board_id: BOARD_ID,
        ...data,
        created_by: 'You',
        vector_clock: { you: 1 },
        created_at: Date.now(),
        updated_at: Date.now(),
      };
      
      setTasks([...tasks, newTask]);

      try {
        const created = await api.createTask({
          board_id: BOARD_ID,
          ...data,
          created_by: 'You',
        });
        setTasks(prev => {
          // Replace temp task with created task
          const updated = prev.map(t => t.task_id === tempId ? created : t);
          // Remove duplicates (WebSocket may have added it first)
          return updated.filter((t, index, arr) => 
            arr.findIndex(x => x.task_id === t.task_id) === index
          );
        });
        showToast('Task created successfully', 'success');
      } catch {
        showToast('Task created locally - backend unavailable', 'info');
      }
    }
  };

  const todoTasks = tasks.filter(t => t.column === ColumnType.TODO);
  const inProgressTasks = tasks.filter(t => t.column === ColumnType.IN_PROGRESS);
  const doneTasks = tasks.filter(t => t.column === ColumnType.DONE);

  if (isLoading) {
    return (
      <div className="min-h-screen flex items-center justify-center" style={{ backgroundColor: '#222831' }}>
        <div style={{ color: '#00ADB5' }}>Loading board...</div>
      </div>
    );
  }

  return (
    <div className="min-h-screen p-6" style={{ backgroundColor: '#222831' }}>
      <header className="mb-6 flex justify-between items-center">
        <div>
          <h1 className="text-3xl font-bold mb-2" style={{ color: '#00ADB5' }}>
            Project Management Board
          </h1>
          <button
            onClick={handleCreateTask}
            className="px-4 py-2 rounded font-semibold hover:opacity-80 transition-opacity"
            style={{ backgroundColor: '#00ADB5', color: '#222831' }}
          >
            + New Task
          </button>
        </div>
        <ConnectionStatus status={wsStatus} />
      </header>

      <main className="flex gap-4 overflow-x-auto">
        <Column 
          title="To Do" 
          tasks={todoTasks} 
          columnId={ColumnType.TODO} 
          onEditTask={handleEditTask}
          remoteUpdateIds={remoteUpdateIds}
        />
        <Column 
          title="In Progress" 
          tasks={inProgressTasks} 
          columnId={ColumnType.IN_PROGRESS} 
          onEditTask={handleEditTask}
          remoteUpdateIds={remoteUpdateIds}
        />
        <Column 
          title="Done" 
          tasks={doneTasks} 
          columnId={ColumnType.DONE} 
          onEditTask={handleEditTask}
          remoteUpdateIds={remoteUpdateIds}
        />
      </main>

      <TaskModal
        key={editingTask?.task_id ?? 'new'}
        isOpen={isModalOpen}
        task={editingTask}
        onClose={() => setIsModalOpen(false)}
        onSave={handleSaveTask}
      />
    </div>
  );
}
