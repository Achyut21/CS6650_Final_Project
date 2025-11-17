import type { Task } from '@/types/task';
import { TaskCard } from './TaskCard';

interface ColumnProps {
  title: string;
  tasks: Task[];
  onEditTask: (task: Task) => void;
}

/**
 * Column component representing a task status (To Do, In Progress, Done)
 */
export function Column({ title, tasks, onEditTask }: ColumnProps) {
  return (
    <section
      className="flex-1 min-w-[300px] p-4 rounded-lg"
      style={{ backgroundColor: '#393E46' }}
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
        {tasks.map((task) => (
          <TaskCard key={task.task_id} task={task} onEdit={onEditTask} />
        ))}
      </div>
    </section>
  );
}
