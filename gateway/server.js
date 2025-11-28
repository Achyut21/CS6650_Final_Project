const express = require('express');
const http = require('http');
const socketIo = require('socket.io');
const net = require('net');
const path = require('path');
const cors = require('cors');

// Configuration
// Usage: node server.js [master_machine] [backup_machine]
// Local:  node server.js
// Khoury: node server.js 81 82
const PORT = 8080;
const MASTER_HOST = process.argv[2] ? `10.200.125.${parseInt(process.argv[2])}` : '127.0.0.1';
const MASTER_PORT = 12345;
const BACKUP_HOST = process.argv[3] ? `10.200.125.${parseInt(process.argv[3])}` : '127.0.0.1';
const BACKUP_PORT = 12346;

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
  DELETE_TASK: 3,
  GET_BOARD: 4
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
        // Response is now 4 integers: success, conflict, rejected, task_id
        if (responseData.length >= 16) {
          const success = responseData.readInt32BE(0) === 1;
          const conflict = responseData.readInt32BE(4) === 1;
          const rejected = responseData.readInt32BE(8) === 1;
          const taskId = responseData.readInt32BE(12);
          
          console.log('[DEBUG] Response - success:', success, 'conflict:', conflict, 'rejected:', rejected);
          resolve({ success, conflict, rejected, taskId });
        } else {
          // just success boolean
          const success = responseData.readInt32BE(0) === 1;
          console.log('[DEBUG] Success value:', success);
          resolve({ success, conflict: false, rejected: false });
        }
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
  const buffer = Buffer.alloc(2048); // More space for all fields
  let offset = 0;
  
  // task_id (4 bytes)
  buffer.writeInt32BE(task.task_id || 0, offset);
  offset += 4;
  
  // title length (4 bytes) + title
  const title = task.title || '';
  const titleLen = Buffer.byteLength(title, 'utf8');
  buffer.writeInt32BE(titleLen, offset);
  offset += 4;
  buffer.write(title, offset, titleLen, 'utf8');
  offset += titleLen;
  
  // description length (4 bytes) + description
  const desc = task.description || '';
  const descLen = Buffer.byteLength(desc, 'utf8');
  buffer.writeInt32BE(descLen, offset);
  offset += 4;
  buffer.write(desc, offset, descLen, 'utf8');
  offset += descLen;
  
  // board_id length (4 bytes) + board_id
  const boardId = task.board_id || 'board-1';
  const boardIdLen = Buffer.byteLength(boardId, 'utf8');
  buffer.writeInt32BE(boardIdLen, offset);
  offset += 4;
  buffer.write(boardId, offset, boardIdLen, 'utf8');
  offset += boardIdLen;
  
  // created_by length (4 bytes) + created_by
  const createdBy = task.created_by || 'user';
  const createdByLen = Buffer.byteLength(createdBy, 'utf8');
  buffer.writeInt32BE(createdByLen, offset);
  offset += 4;
  buffer.write(createdBy, offset, createdByLen, 'utf8');
  offset += createdByLen;
  
  // column (4 bytes)
  buffer.writeInt32BE(task.column || 0, offset);
  offset += 4;
  
  // client_id (4 bytes)
  buffer.writeInt32BE(task.client_id || 0, offset);
  offset += 4;
  
  // created_at (8 bytes) - will be set by backend, send 0
  buffer.writeBigInt64BE(BigInt(task.created_at || 0), offset);
  offset += 8;
  
  // updated_at (8 bytes) - will be set by backend, send 0
  buffer.writeBigInt64BE(BigInt(task.updated_at || 0), offset);
  offset += 8;
  
  // vector clock size (0 for now)
  buffer.writeInt32BE(0, offset);
  offset += 4;
  
  return buffer.slice(0, offset);
}

// Deserialize task from binary format
function deserializeTask(buffer, offset = 0) {
  let pos = offset;
  
  // task_id (4 bytes)
  const task_id = buffer.readInt32BE(pos);
  pos += 4;
  
  // title length + title
  const titleLen = buffer.readInt32BE(pos);
  pos += 4;
  const title = buffer.toString('utf8', pos, pos + titleLen);
  pos += titleLen;
  
  // description length + description
  const descLen = buffer.readInt32BE(pos);
  pos += 4;
  const description = buffer.toString('utf8', pos, pos + descLen);
  pos += descLen;
  
  // board_id length + board_id
  const boardIdLen = buffer.readInt32BE(pos);
  pos += 4;
  const board_id = buffer.toString('utf8', pos, pos + boardIdLen);
  pos += boardIdLen;
  
  // created_by length + created_by
  const createdByLen = buffer.readInt32BE(pos);
  pos += 4;
  const created_by = buffer.toString('utf8', pos, pos + createdByLen);
  pos += createdByLen;
  
  // column (4 bytes)
  const column = buffer.readInt32BE(pos);
  pos += 4;
  
  // client_id (4 bytes)
  const client_id = buffer.readInt32BE(pos);
  pos += 4;
  
  // created_at (8 bytes)
  const created_at = Number(buffer.readBigInt64BE(pos));
  pos += 8;
  
  // updated_at (8 bytes)
  const updated_at = Number(buffer.readBigInt64BE(pos));
  pos += 8;
  
  // vector clock size
  const clockSize = buffer.readInt32BE(pos);
  pos += 4;
  
  // skip vector clock data for now
  pos += clockSize * 8; // each entry: process_id (4) + count (4)
  
  return {
    task: {
      task_id,
      board_id,
      title,
      description,
      column,
      created_by,
      vector_clock: {},
      created_at,
      updated_at
    },
    bytesRead: pos - offset
  };
}

// Get all tasks from backend (with failover support)
async function getBoardFromBackend(retryWithBackup = true) {
  return new Promise((resolve, reject) => {
    const client = new net.Socket();
    let responseData = Buffer.alloc(0);
    
    const host = currentBackendHost;
    const port = currentBackendPort;
    
    client.connect(port, host, () => {
      try {
        console.log('[GET_BOARD] Requesting all tasks from backend at', host + ':' + port);
        
        // Send GET_BOARD operation type
        const opBuffer = Buffer.alloc(4);
        opBuffer.writeInt32BE(OpType.GET_BOARD, 0);
        client.write(opBuffer);
        
        // Send empty task (required by protocol but unused)
        const emptyTask = serializeTask({ task_id: 0, description: '', column: 0, client_id: 0 });
        const sizeBuffer = Buffer.alloc(4);
        sizeBuffer.writeInt32BE(emptyTask.length, 0);
        client.write(sizeBuffer);
        client.write(emptyTask);
        
        client.end();
      } catch (err) {
        client.destroy();
        reject(err);
      }
    });
    
    client.on('data', (data) => {
      responseData = Buffer.concat([responseData, data]);
    });
    
    client.on('end', () => {
      try {
        console.log('[GET_BOARD] Received response:', responseData.length, 'bytes');
        
        // Read count (4 bytes)
        const count = responseData.readInt32BE(0);
        console.log('[GET_BOARD] Task count:', count);
        
        const tasks = [];
        let offset = 4;
        
        // Read each task
        for (let i = 0; i < count; i++) {
          // Read size
          const size = responseData.readInt32BE(offset);
          offset += 4;
          
          // Deserialize task
          const result = deserializeTask(responseData, offset);
          tasks.push(result.task);
          offset += result.bytesRead;
        }
        
        console.log('[GET_BOARD] Parsed', tasks.length, 'tasks');
        resolve(tasks);
      } catch (err) {
        console.error('[GET_BOARD] Error parsing response:', err);
        reject(err);
      }
    });
    
    client.on('error', (err) => {
      console.error('[GET_BOARD] Socket error:', err.message);
      
      // If master fails and we haven't failed over yet, try backup
      if (!failedOverToBackup && retryWithBackup && host === MASTER_HOST) {
        console.log('[GET_BOARD FAILOVER] Master failed, switching to backup...');
        currentBackendHost = BACKUP_HOST;
        currentBackendPort = BACKUP_PORT;
        failedOverToBackup = true;
        
        // Retry with backup
        getBoardFromBackend(false)
          .then(resolve)
          .catch(reject);
      } else {
        reject(err);
      }
    });
    
    client.setTimeout(5000, () => {
      client.destroy();
      
      // Also try failover on timeout
      if (!failedOverToBackup && retryWithBackup) {
        console.log('[GET_BOARD FAILOVER] Timeout, switching to backup...');
        currentBackendHost = BACKUP_HOST;
        currentBackendPort = BACKUP_PORT;
        failedOverToBackup = true;
        
        getBoardFromBackend(false)
          .then(resolve)
          .catch(reject);
      } else {
        reject(new Error('GET_BOARD timeout'));
      }
    });
  });
}

// REST API Endpoints

// GET /api/boards/:id - Get all tasks for a board
app.get('/api/boards/:id', async (req, res) => {
  try {
    const tasks = await getBoardFromBackend();
    res.json({
      board_id: req.params.id,
      tasks: tasks
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
      title: title || '',
      description: description || '',
      board_id: board_id || 'board-1',
      created_by: created_by || 'user',
      column: column || Column.TODO,
      client_id: 1 // Simple client ID for now
    };
    
    const result = await sendToBackend(OpType.CREATE_TASK, taskData);
    
    if (result.success) {
      // Use task ID from backend response
      const taskId = result.taskId;
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
        title: '',
        description: '',
        board_id: 'board-1',
        created_by: 'user',
        column: column,
        client_id: 1
      };
    } else {
      // Update operation (title and/or description)
      console.log('[PATCH] Detected as UPDATE_TASK');
      opType = OpType.UPDATE_TASK;
      taskData = {
        task_id: taskId,
        title: title || '',
        description: description || '',
        board_id: 'board-1',
        created_by: 'user',
        column: Column.TODO,
        client_id: 1
      };
    }
    
    const result = await sendToBackend(opType, taskData);
    
    if (result.success) {
      const updatedTask = {
        task_id: taskId,
        ...req.body,
        updated_at: Date.now(),
        conflict: result.conflict || false,
        rejected: result.rejected || false
      };
      
      if (column !== undefined) {
        io.emit('TASK_MOVED', { task: updatedTask });
      } else {
        io.emit('TASK_UPDATED', { task: updatedTask });
      }
      
      // Add conflict warning to response if detected
      if (result.conflict) {
        updatedTask.warning = 'Concurrent edit detected - applied with last-write-wins';
      }
      if (result.rejected) {
        return res.status(409).json({ 
          error: 'Update rejected - operation was outdated',
          task: updatedTask 
        });
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
      title: '',
      description: '',
      board_id: 'board-1',
      created_by: 'user',
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
  console.log(`Backup backend: ${BACKUP_HOST}:${BACKUP_PORT}`);
  console.log(`WebSocket server ready`);
});
