import { useEffect, useRef, useState } from 'react';
import { draggable } from '@atlaskit/pragmatic-drag-and-drop/element/adapter';
import type { Task } from '@/types/task';

interface TaskCardProps {
  task: Task;
  index: number;
  onEdit: (task: Task) => void;
  isRemoteUpdate?: boolean;
}

/**
 * Draggable task card component
 */
export function TaskCard({ task, index, onEdit, isRemoteUpdate = false }: TaskCardProps) {
  const ref = useRef<HTMLDivElement>(null);
  const [isDragging, setIsDragging] = useState(false);

  useEffect(() => {
    const element = ref.current;
    if (!element) return;

    return draggable({
      element,
      getInitialData: () => ({ task, index, type: 'task' }),
      onDragStart: () => setIsDragging(true),
      onDrop: () => setIsDragging(false),
    });
  }, [task, index]);

  return (
    <div
      ref={ref}
      className={`p-4 rounded-lg shadow-md cursor-grab hover:shadow-lg transition-all ${
        isRemoteUpdate ? 'animate-pulse-update' : ''
      }`}
      style={{
        backgroundColor: isDragging ? '#00ADB5' : '#222831',
        opacity: isDragging ? 0.5 : 1,
      }}
      onClick={() => !isDragging && onEdit(task)}
    >
      <h3 className="text-lg font-semibold mb-2" style={{ color: '#EEEEEE' }}>
        {task.title}
      </h3>
      {task.description && (
        <p className="text-sm" style={{ color: '#EEEEEE', opacity: 0.8 }}>
          {task.description}
        </p>
      )}
      <div className="mt-3 text-xs" style={{ color: '#EEEEEE', opacity: 0.6 }}>
        Created by {task.created_by}
      </div>
    </div>
  );
}
