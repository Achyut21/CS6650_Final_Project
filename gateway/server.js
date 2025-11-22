const express = require('express');
const http = require('http');
const socketIo = require('socket.io');
const net = require('net');
const path = require('path');
const cors = require('cors');

// Configuration
const PORT = 8080;
const MASTER_HOST = 'localhost';
const MASTER_PORT = 12345;
const BACKUP_HOST = 'localhost';
const BACKUP_PORT = 12346;

// Simple task ID counter
let nextTaskId = 0;

// Failover state
let currentBackendHost = MASTER_HOST;
let currentBackendPort = MASTER_PORT;
let failedOverToBackup = false;

// Create Express app
const app = express();
const server = http.createServer(app);
const io = socketIo(server, {
  cors: { origin: '*' }
});

// Middleware
app.use(cors());
app.use(express.json());
app.use(express.static(path.join(__dirname, 'public')));

// OpType enum (matching C++ backend)
const OpType = {
  CREATE_TASK: 0,
  UPDATE_TASK: 1,
  MOVE_TASK: 2,
  DELETE_TASK: 3
};

// Column enum
const Column = {
  TODO: 0,
  IN_PROGRESS: 1,
  DONE: 2
};

// Helper: Send request to C++ backend (master or backup after failover)
async function sendToBackend(opType, taskData, retryWithBackup = true) {
  return new Promise((resolve, reject) => {
    const client = new net.Socket();
    let responseData = Buffer.alloc(0);
    
    const host = currentBackendHost;
    const port = currentBackendPort;
    
    client.connect(port, host, () => {
      try {
        console.log('[DEBUG] Connected to master, sending operation...');
        
        // Send operation type (4 bytes)
        const opBuffer = Buffer.alloc(4);
        opBuffer.writeInt32BE(opType, 0);
        client.write(opBuffer);
        console.log('[DEBUG] Sent opType:', opType);
        
        // Serialize task data
        const taskBuffer = serializeTask(taskData);
        console.log('[DEBUG] Task buffer size:', taskBuffer.length);
        
        // Send size of task data (4 bytes)
        const sizeBuffer = Buffer.alloc(4);
        sizeBuffer.writeInt32BE(taskBuffer.length, 0);
        client.write(sizeBuffer);
        console.log('[DEBUG] Sent size:', taskBuffer.length);
        
        // Send task data
        client.write(taskBuffer);
        console.log('[DEBUG] Sent task data');
        
        // End the write stream to signal completion
        client.end();
        console.log('[DEBUG] Connection ended, waiting for response...');
      } catch (err) {
        client.destroy();
        reject(err);
      }
    });
    
    client.on('data', (data) => {
      console.log('[DEBUG] Received data chunk:', data.length, 'bytes');
      responseData = Buffer.concat([responseData, data]);
    });
    
    client.on('end', () => {
      console.log('[DEBUG] Connection ended, total response:', responseData.length, 'bytes');
      try {
        const success = responseData.readInt32BE(0) === 1;
        console.log('[DEBUG] Success value:', success);
        resolve({ success });
      } catch (err) {
        console.error('[DEBUG] Error parsing response:', err);
        reject(err);
      }
    });
    
    client.on('error', (err) => {
      console.error('[DEBUG] Socket error:', err.message);
      
      // If master fails and we haven't failed over yet, try backup
      if (!failedOverToBackup && retryWithBackup && host === MASTER_HOST) {
        console.log('[FAILOVER] Master failed, switching to backup...');
        currentBackendHost = BACKUP_HOST;
        currentBackendPort = BACKUP_PORT;
        failedOverToBackup = true;
        
        // Retry with backup
        sendToBackend(opType, taskData, false)
          .then(resolve)
          .catch(reject);
      } else {
        reject(err);
      }
    });
    
    client.setTimeout(5000, () => {
      client.destroy();
      reject(new Error('Connection timeout'));
    });
  });
}

// Serialize task to binary format for C++ backend
function serializeTask(task) {
  const buffer = Buffer.alloc(1024); // Enough space
  let offset = 0;
  
  // task_id (4 bytes)
  buffer.writeInt32BE(task.task_id || 0, offset);
  offset += 4;
  
  // description length (4 bytes) + description
  const desc = task.description || task.title || '';
  const descLen = Buffer.byteLength(desc, 'utf8');
  buffer.writeInt32BE(descLen, offset);
  offset += 4;
  buffer.write(desc, offset, descLen, 'utf8');
  offset += descLen;
  
  // column (4 bytes)
  buffer.writeInt32BE(task.column || 0, offset);
  offset += 4;
  
  // client_id (4 bytes)
  buffer.writeInt32BE(task.client_id || 0, offset);
  offset += 4;
  
  // vector clock size (0 for now)
  buffer.writeInt32BE(0, offset);
  offset += 4;
  
  return buffer.slice(0, offset);
}

// REST API Endpoints

// GET /api/boards/:id - Get all tasks for a board
app.get('/api/boards/:id', async (req, res) => {
  try {
    // For now, return mock empty board
    // In future, implement GET_BOARD operation in C++ backend
    res.json({
      board_id: req.params.id,
      tasks: []
    });
  } catch (err) {
    console.error('Error fetching board:', err);
    res.status(500).json({ error: 'Failed to fetch board' });
  }
});

// POST /api/tasks - Create a new task
app.post('/api/tasks', async (req, res) => {
  try {
    const { title, description, column, created_by, board_id } = req.body;
    
    const taskData = {
      task_id: 0, // Will be assigned by backend
      description: title || description,
      column: column || Column.TODO,
      client_id: 1 // Simple client ID for now
    };
    
    const result = await sendToBackend(OpType.CREATE_TASK, taskData);
    
    if (result.success) {
      // Broadcast to all connected clients
      const taskId = nextTaskId++;
      const createdTask = {
        task_id: taskId,
        board_id: board_id || 'board-1',
        title: title || description,
        description: description || '',
        column: column || Column.TODO,
        created_by: created_by || 'user',
        vector_clock: {},
        created_at: Date.now(),
        updated_at: Date.now()
      };
      
      io.emit('TASK_CREATED', { task: createdTask });
      res.status(201).json(createdTask);
    } else {
      res.status(500).json({ error: 'Failed to create task' });
    }
  } catch (err) {
    console.error('Error creating task:', err);
    res.status(500).json({ error: 'Failed to create task' });
  }
});

// PATCH /api/tasks/:id - Update a task
app.patch('/api/tasks/:id', async (req, res) => {
  try {
    const taskId = parseInt(req.params.id);
    const { title, description, column } = req.body;
    
    console.log('[PATCH] Task', taskId, '- Body:', { title, description, column });
    
    let opType, taskData;
    
    // Determine operation type based on what's being updated
    // If ONLY column is changing (and title/description are NOT provided), it's a MOVE
    // Otherwise, it's an UPDATE
    const isColumnOnlyUpdate = (column !== undefined) && (title === undefined) && (description === undefined);
    
    if (isColumnOnlyUpdate) {
      // Move operation
      console.log('[PATCH] Detected as MOVE_TASK');
      opType = OpType.MOVE_TASK;
      taskData = {
        task_id: taskId,
        column: column,
        description: '',
        client_id: 1
      };
    } else {
      // Update operation (title and/or description)
      console.log('[PATCH] Detected as UPDATE_TASK');
      opType = OpType.UPDATE_TASK;
      taskData = {
        task_id: taskId,
        description: title || description || '',
        column: Column.TODO,
        client_id: 1
      };
    }
    
    const result = await sendToBackend(opType, taskData);
    
    if (result.success) {
      const updatedTask = {
        task_id: taskId,
        ...req.body,
        updated_at: Date.now()
      };
      
      if (column !== undefined) {
        io.emit('TASK_MOVED', { task: updatedTask });
      } else {
        io.emit('TASK_UPDATED', { task: updatedTask });
      }
      
      res.json(updatedTask);
    } else {
      res.status(404).json({ error: 'Task not found' });
    }
  } catch (err) {
    console.error('Error updating task:', err);
    res.status(500).json({ error: 'Failed to update task' });
  }
});

// DELETE /api/tasks/:id - Delete a task
app.delete('/api/tasks/:id', async (req, res) => {
  try {
    const taskId = parseInt(req.params.id);
    
    const taskData = {
      task_id: taskId,
      description: '',
      column: Column.TODO,
      client_id: 1
    };
    
    const result = await sendToBackend(OpType.DELETE_TASK, taskData);
    
    if (result.success) {
      io.emit('TASK_DELETED', { task_id: taskId });
      res.status(204).send();
    } else {
      res.status(404).json({ error: 'Task not found' });
    }
  } catch (err) {
    console.error('Error deleting task:', err);
    res.status(500).json({ error: 'Failed to delete task' });
  }
});

// WebSocket connection handling
io.on('connection', (socket) => {
  console.log('Client connected:', socket.id);
  
  socket.on('disconnect', () => {
    console.log('Client disconnected:', socket.id);
  });
});

// Serve React app for all other routes
app.get('*', (req, res) => {
  res.sendFile(path.join(__dirname, 'public', 'index.html'));
});

// Start server
server.listen(PORT, () => {
  console.log(`API Gateway listening on port ${PORT}`);
  console.log(`Master backend: ${MASTER_HOST}:${MASTER_PORT}`);
  console.log(`WebSocket server ready`);
});
