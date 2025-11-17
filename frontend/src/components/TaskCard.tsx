import type { Task } from '@/types/task';

interface TaskCardProps {
  task: Task;
  onEdit: (task: Task) => void;
}

/**
 * Individual task card component
 */
export function TaskCard({ task, onEdit }: TaskCardProps) {
  return (
    <div
      className="p-4 rounded-lg shadow-md cursor-pointer hover:shadow-lg transition-shadow"
      style={{ backgroundColor: '#393E46' }}
      onClick={() => onEdit(task)}
    >
      <h3 className="text-lg font-semibold mb-2" style={{ color: '#00ADB5' }}>
        {task.title}
      </h3>
      {task.description && (
        <p className="text-sm" style={{ color: '#EEEEEE' }}>
          {task.description}
        </p>
      )}
      <div className="mt-3 text-xs" style={{ color: '#EEEEEE', opacity: 0.6 }}>
        Created by {task.created_by}
      </div>
    </div>
  );
}
