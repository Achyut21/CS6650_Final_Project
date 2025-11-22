import { useEffect, useRef, useState } from 'react';
import { dropTargetForElements } from '@atlaskit/pragmatic-drag-and-drop/element/adapter';
import type { Task } from '@/types/task';
import { Column as ColumnType } from '@/types/task';
import { TaskCard } from './TaskCard';

interface ColumnProps {
  title: string;
  tasks: Task[];
  columnId: ColumnType;
  onEditTask: (task: Task) => void;
  remoteUpdateIds: Set<number>;
}

/**
 * Column component with drop target support
 */
export function Column({ title, tasks, columnId, onEditTask, remoteUpdateIds }: ColumnProps) {
  const ref = useRef<HTMLElement>(null);
  const [isDraggedOver, setIsDraggedOver] = useState(false);

  useEffect(() => {
    const element = ref.current;
    if (!element) return;

    return dropTargetForElements({
      element,
      getData: () => ({ columnId, type: 'column' }),
      canDrop: ({ source }) => source.data.type === 'task',
      onDragEnter: () => setIsDraggedOver(true),
      onDragLeave: () => setIsDraggedOver(false),
      onDrop: () => setIsDraggedOver(false),
    });
  }, [columnId]);

  return (
    <section
      ref={ref}
      className="flex-1 min-w-[300px] p-4 rounded-lg transition-colors"
      style={{
        backgroundColor: isDraggedOver ? '#00ADB5' : '#393E46',
        minHeight: '200px',
      }}
    >
      <header className="mb-4">
        <h2 className="text-xl font-bold" style={{ color: '#00ADB5' }}>
          {title}
        </h2>
        <div className="text-sm mt-1" style={{ color: '#EEEEEE', opacity: 0.7 }}>
          {tasks.length} {tasks.length === 1 ? 'task' : 'tasks'}
        </div>
      </header>

      <div className="space-y-3">
        {tasks.map((task, index) => (
          <TaskCard 
            key={task.task_id} 
            task={task} 
            index={index} 
            onEdit={onEditTask}
            isRemoteUpdate={remoteUpdateIds.has(task.task_id)}
          />
        ))}
      </div>
    </section>
  );
}
