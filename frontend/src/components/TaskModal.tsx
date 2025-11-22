import { useState } from 'react';
import type { Task } from '@/types/task';
import { Column } from '@/types/task';

interface TaskModalProps {
  isOpen: boolean;
  task?: Task | null;
  onClose: () => void;
  onSave: (data: { title: string; description: string; column: Column }) => void;
}

/**
 * Modal for creating and editing tasks
 * Use key={task?.task_id ?? 'new'} when rendering to reset form state
 */
export function TaskModal({ isOpen, task, onClose, onSave }: TaskModalProps) {
  // Initialize from props - component remounts when key changes
  const [title, setTitle] = useState(task?.title ?? '');
  const [description, setDescription] = useState(task?.description ?? '');
  const [column, setColumn] = useState<Column>(task?.column ?? Column.TODO);

  if (!isOpen) return null;

  const handleSubmit = (e: React.FormEvent) => {
    e.preventDefault();
    if (!title.trim()) return;
    onSave({ title, description, column });
    onClose();
  };

  return (
    <div
      className="fixed inset-0 flex items-center justify-center z-50"
      style={{ backgroundColor: 'rgba(34, 40, 49, 0.8)' }}
      onClick={onClose}
    >
      <div
        className="p-6 rounded-lg shadow-xl w-full max-w-md"
        style={{ backgroundColor: '#393E46' }}
        onClick={(e) => e.stopPropagation()}
      >
        <h2 className="text-2xl font-bold mb-4" style={{ color: '#00ADB5' }}>
          {task ? 'Edit Task' : 'Create Task'}
        </h2>

        <form onSubmit={handleSubmit} className="space-y-4">
          <div>
            <label className="block text-sm font-medium mb-2" style={{ color: '#EEEEEE' }}>
              Title
            </label>
            <input
              type="text"
              value={title}
              onChange={(e) => setTitle(e.target.value)}
              className="w-full px-3 py-2 rounded"
              style={{ backgroundColor: '#222831', color: '#EEEEEE', border: '1px solid #00ADB5' }}
              placeholder="Enter task title"
              autoFocus
              required
            />
          </div>

          <div>
            <label className="block text-sm font-medium mb-2" style={{ color: '#EEEEEE' }}>
              Description
            </label>
            <textarea
              value={description}
              onChange={(e) => setDescription(e.target.value)}
              className="w-full px-3 py-2 rounded min-h-[100px]"
              style={{ backgroundColor: '#222831', color: '#EEEEEE', border: '1px solid #00ADB5' }}
              placeholder="Enter task description"
            />
          </div>

          <div>
            <label className="block text-sm font-medium mb-2" style={{ color: '#EEEEEE' }}>
              Status
            </label>
            <select
              value={column}
              onChange={(e) => setColumn(Number(e.target.value) as Column)}
              className="w-full px-3 py-2 rounded"
              style={{ backgroundColor: '#222831', color: '#EEEEEE', border: '1px solid #00ADB5' }}
            >
              <option value={Column.TODO}>To Do</option>
              <option value={Column.IN_PROGRESS}>In Progress</option>
              <option value={Column.DONE}>Done</option>
            </select>
          </div>

          <div className="flex gap-3 justify-end pt-2">
            <button
              type="button"
              onClick={onClose}
              className="px-4 py-2 rounded hover:opacity-80 transition-opacity"
              style={{ backgroundColor: '#222831', color: '#EEEEEE' }}
            >
              Cancel
            </button>
            <button
              type="submit"
              className="px-4 py-2 rounded font-semibold hover:opacity-80 transition-opacity"
              style={{ backgroundColor: '#00ADB5', color: '#222831' }}
            >
              {task ? 'Update' : 'Create'}
            </button>
          </div>
        </form>
      </div>
    </div>
  );
}
