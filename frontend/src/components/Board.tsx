import { useState } from 'react';
import type { Task } from '@/types/task';
import { Column as ColumnType } from '@/types/task';
import { Column } from './Column';
import { TaskModal } from './TaskModal';

// Sample tasks for testing
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
 * Main kanban board component
 */
export function Board() {
  const [tasks, setTasks] = useState<Task[]>(SAMPLE_TASKS);
  const [isModalOpen, setIsModalOpen] = useState(false);
  const [editingTask, setEditingTask] = useState<Task | null>(null);

  const handleEditTask = (task: Task) => {
    setEditingTask(task);
    setIsModalOpen(true);
  };

  const handleCreateTask = () => {
    setEditingTask(null);
    setIsModalOpen(true);
  };

  const handleSaveTask = (data: { title: string; description: string; column: ColumnType }) => {
    if (editingTask) {
      // Update existing task
      setTasks(tasks.map(t =>
        t.task_id === editingTask.task_id
          ? { ...t, ...data, updated_at: Date.now() }
          : t
      ));
    } else {
      // Create new task
      const newTask: Task = {
        task_id: Date.now(),
        board_id: 'board-1',
        title: data.title,
        description: data.description,
        column: data.column,
        created_by: 'You',
        vector_clock: { you: 1 },
        created_at: Date.now(),
        updated_at: Date.now(),
      };
      setTasks([...tasks, newTask]);
    }
  };

  // Group tasks by column
  const todoTasks = tasks.filter(t => t.column === ColumnType.TODO);
  const inProgressTasks = tasks.filter(t => t.column === ColumnType.IN_PROGRESS);
  const doneTasks = tasks.filter(t => t.column === ColumnType.DONE);

  return (
    <div className="min-h-screen p-6" style={{ backgroundColor: '#222831' }}>
      <header className="mb-6">
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
      </header>

      <main className="flex gap-4 overflow-x-auto">
        <Column title="To Do" tasks={todoTasks} onEditTask={handleEditTask} />
        <Column title="In Progress" tasks={inProgressTasks} onEditTask={handleEditTask} />
        <Column title="Done" tasks={doneTasks} onEditTask={handleEditTask} />
      </main>

      <TaskModal
        isOpen={isModalOpen}
        task={editingTask}
        onClose={() => setIsModalOpen(false)}
        onSave={handleSaveTask}
      />
    </div>
  );
}
