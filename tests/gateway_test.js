/**
 * Gateway API Tests
 * Tests the Node.js gateway API endpoints and serialization
 * 
 * Usage: node gateway_test.js
 * Requires: Backend master running on port 12345
 */

const http = require('http');

const API_HOST = 'localhost';
const API_PORT = 8080;

let testsPass = 0;
let testsFail = 0;

// Helper functions
function log(msg) {
    console.log(`[INFO] ${msg}`);
}

function pass(testName) {
    console.log(`\x1b[32m[PASS]\x1b[0m ${testName}`);
    testsPass++;
}

function fail(testName, reason) {
    console.log(`\x1b[31m[FAIL]\x1b[0m ${testName}: ${reason}`);
    testsFail++;
}

// HTTP request helper
function request(method, path, body = null) {
    return new Promise((resolve, reject) => {
        const options = {
            hostname: API_HOST,
            port: API_PORT,
            path: path,
            method: method,
            headers: {
                'Content-Type': 'application/json',
            },
            timeout: 5000,
        };

        const req = http.request(options, (res) => {
            let data = '';
            res.on('data', (chunk) => data += chunk);
            res.on('end', () => {
                try {
                    const json = data ? JSON.parse(data) : null;
                    resolve({ status: res.statusCode, data: json, raw: data });
                } catch (e) {
                    resolve({ status: res.statusCode, data: null, raw: data });
                }
            });
        });

        req.on('error', reject);
        req.on('timeout', () => {
            req.destroy();
            reject(new Error('Request timeout'));
        });

        if (body) {
            req.write(JSON.stringify(body));
        }
        req.end();
    });
}

// Test functions
async function testCreateTask() {
    const testName = 'Create task with all fields';
    try {
        const response = await request('POST', '/api/tasks', {
            title: 'Test Task',
            description: 'Test Description',
            column: 0,
            created_by: 'testuser',
            board_id: 'board-1'
        });

        if (response.status === 201 && response.data && response.data.task_id !== undefined) {
            pass(testName);
            return response.data.task_id;
        } else {
            fail(testName, `Status: ${response.status}, Data: ${JSON.stringify(response.data)}`);
            return null;
        }
    } catch (e) {
        fail(testName, e.message);
        return null;
    }
}

async function testCreateTaskMinimal() {
    const testName = 'Create task with minimal fields';
    try {
        const response = await request('POST', '/api/tasks', {
            title: 'Minimal Task',
            column: 0
        });

        if (response.status === 201 && response.data && response.data.task_id !== undefined) {
            pass(testName);
            return response.data.task_id;
        } else {
            fail(testName, `Status: ${response.status}`);
            return null;
        }
    } catch (e) {
        fail(testName, e.message);
        return null;
    }
}

async function testCreateTaskEmptyTitle() {
    const testName = 'Create task with empty title';
    try {
        const response = await request('POST', '/api/tasks', {
            title: '',
            description: 'No title task',
            column: 0
        });

        if (response.status === 201) {
            pass(testName);
            return response.data.task_id;
        } else {
            fail(testName, `Status: ${response.status}`);
            return null;
        }
    } catch (e) {
        fail(testName, e.message);
        return null;
    }
}

async function testCreateTaskLongContent() {
    const testName = 'Create task with long content';
    try {
        const longTitle = 'A'.repeat(200);
        const longDesc = 'B'.repeat(500);
        
        const response = await request('POST', '/api/tasks', {
            title: longTitle,
            description: longDesc,
            column: 0,
            created_by: 'longuser'
        });

        if (response.status === 201 && response.data) {
            pass(testName);
            return response.data.task_id;
        } else {
            fail(testName, `Status: ${response.status}`);
            return null;
        }
    } catch (e) {
        fail(testName, e.message);
        return null;
    }
}

async function testCreateTaskAllColumns() {
    const testName = 'Create tasks in all columns';
    try {
        const r1 = await request('POST', '/api/tasks', { title: 'TODO', column: 0 });
        const r2 = await request('POST', '/api/tasks', { title: 'IN_PROGRESS', column: 1 });
        const r3 = await request('POST', '/api/tasks', { title: 'DONE', column: 2 });

        if (r1.status === 201 && r2.status === 201 && r3.status === 201) {
            pass(testName);
        } else {
            fail(testName, `Statuses: ${r1.status}, ${r2.status}, ${r3.status}`);
        }
    } catch (e) {
        fail(testName, e.message);
    }
}

async function testGetBoard() {
    const testName = 'Get board';
    try {
        const response = await request('GET', '/api/boards/board-1');

        if (response.status === 200 && response.data && response.data.tasks) {
            pass(testName);
            return response.data;
        } else {
            fail(testName, `Status: ${response.status}`);
            return null;
        }
    } catch (e) {
        fail(testName, e.message);
        return null;
    }
}

async function testUpdateTaskTitle(taskId) {
    const testName = 'Update task title';
    if (!taskId) {
        fail(testName, 'No task ID provided');
        return;
    }
    
    try {
        const response = await request('PATCH', `/api/tasks/${taskId}`, {
            title: 'Updated Title'
        });

        if (response.status === 200 && response.data) {
            pass(testName);
        } else {
            fail(testName, `Status: ${response.status}`);
        }
    } catch (e) {
        fail(testName, e.message);
    }
}

async function testUpdateTaskDescription(taskId) {
    const testName = 'Update task description';
    if (!taskId) {
        fail(testName, 'No task ID provided');
        return;
    }
    
    try {
        const response = await request('PATCH', `/api/tasks/${taskId}`, {
            description: 'Updated Description'
        });

        if (response.status === 200) {
            pass(testName);
        } else {
            fail(testName, `Status: ${response.status}`);
        }
    } catch (e) {
        fail(testName, e.message);
    }
}

async function testUpdateTaskBoth(taskId) {
    const testName = 'Update task title and description';
    if (!taskId) {
        fail(testName, 'No task ID provided');
        return;
    }
    
    try {
        const response = await request('PATCH', `/api/tasks/${taskId}`, {
            title: 'New Title',
            description: 'New Description'
        });

        if (response.status === 200) {
            pass(testName);
        } else {
            fail(testName, `Status: ${response.status}`);
        }
    } catch (e) {
        fail(testName, e.message);
    }
}

async function testUpdateNonexistentTask() {
    const testName = 'Update non-existent task';
    try {
        const response = await request('PATCH', '/api/tasks/99999', {
            title: 'Should fail'
        });

        if (response.status === 404) {
            pass(testName);
        } else {
            fail(testName, `Expected 404, got ${response.status}`);
        }
    } catch (e) {
        fail(testName, e.message);
    }
}

async function testMoveTask(taskId) {
    const testName = 'Move task to IN_PROGRESS';
    if (!taskId) {
        fail(testName, 'No task ID provided');
        return;
    }
    
    try {
        const response = await request('PATCH', `/api/tasks/${taskId}`, {
            column: 1
        });

        if (response.status === 200) {
            pass(testName);
        } else {
            fail(testName, `Status: ${response.status}`);
        }
    } catch (e) {
        fail(testName, e.message);
    }
}

async function testMoveTaskToDone(taskId) {
    const testName = 'Move task to DONE';
    if (!taskId) {
        fail(testName, 'No task ID provided');
        return;
    }
    
    try {
        const response = await request('PATCH', `/api/tasks/${taskId}`, {
            column: 2
        });

        if (response.status === 200) {
            pass(testName);
        } else {
            fail(testName, `Status: ${response.status}`);
        }
    } catch (e) {
        fail(testName, e.message);
    }
}

async function testMoveTaskBackToTodo(taskId) {
    const testName = 'Move task back to TODO';
    if (!taskId) {
        fail(testName, 'No task ID provided');
        return;
    }
    
    try {
        const response = await request('PATCH', `/api/tasks/${taskId}`, {
            column: 0
        });

        if (response.status === 200) {
            pass(testName);
        } else {
            fail(testName, `Status: ${response.status}`);
        }
    } catch (e) {
        fail(testName, e.message);
    }
}

async function testDeleteTask(taskId) {
    const testName = 'Delete task';
    if (!taskId) {
        fail(testName, 'No task ID provided');
        return;
    }
    
    try {
        const response = await request('DELETE', `/api/tasks/${taskId}`);

        if (response.status === 204 || response.status === 200) {
            pass(testName);
        } else {
            fail(testName, `Status: ${response.status}`);
        }
    } catch (e) {
        fail(testName, e.message);
    }
}

async function testDeleteNonexistentTask() {
    const testName = 'Delete non-existent task';
    try {
        const response = await request('DELETE', '/api/tasks/99999');

        if (response.status === 404) {
            pass(testName);
        } else {
            fail(testName, `Expected 404, got ${response.status}`);
        }
    } catch (e) {
        fail(testName, e.message);
    }
}

async function testRapidCreates() {
    const testName = 'Rapid task creation (10 tasks)';
    try {
        const promises = [];
        for (let i = 0; i < 10; i++) {
            promises.push(request('POST', '/api/tasks', {
                title: `Rapid ${i}`,
                description: `Description ${i}`,
                column: i % 3
            }));
        }
        
        const results = await Promise.all(promises);
        const successCount = results.filter(r => r.status === 201).length;
        
        if (successCount >= 8) {
            pass(testName + ` (${successCount}/10)`);
        } else {
            fail(testName, `Only ${successCount}/10 succeeded`);
        }
    } catch (e) {
        fail(testName, e.message);
    }
}

async function testCreateUpdateDelete() {
    const testName = 'Full CRUD cycle';
    try {
        // Create
        const createRes = await request('POST', '/api/tasks', {
            title: 'CRUD Test',
            description: 'Testing full cycle',
            column: 0
        });
        
        if (createRes.status !== 201) {
            fail(testName, 'Create failed');
            return;
        }
        
        const taskId = createRes.data.task_id;
        
        // Update
        const updateRes = await request('PATCH', `/api/tasks/${taskId}`, {
            title: 'Updated CRUD Test'
        });
        
        if (updateRes.status !== 200) {
            fail(testName, 'Update failed');
            return;
        }
        
        // Move
        const moveRes = await request('PATCH', `/api/tasks/${taskId}`, {
            column: 2
        });
        
        if (moveRes.status !== 200) {
            fail(testName, 'Move failed');
            return;
        }
        
        // Delete
        const deleteRes = await request('DELETE', `/api/tasks/${taskId}`);
        
        if (deleteRes.status === 204 || deleteRes.status === 200) {
            pass(testName);
        } else {
            fail(testName, 'Delete failed');
        }
    } catch (e) {
        fail(testName, e.message);
    }
}

async function testSpecialCharacters() {
    const testName = 'Special characters in content';
    try {
        const response = await request('POST', '/api/tasks', {
            title: 'Test <script>alert("xss")</script>',
            description: 'Description with "quotes" and \'apostrophes\'',
            column: 0
        });

        if (response.status === 201) {
            pass(testName);
        } else {
            fail(testName, `Status: ${response.status}`);
        }
    } catch (e) {
        fail(testName, e.message);
    }
}

async function testUnicodeContent() {
    const testName = 'Unicode content';
    try {
        const response = await request('POST', '/api/tasks', {
            title: 'タスク 任务 مهمة',
            description: '描述 설명 विवरण',
            column: 0,
            created_by: '用户'
        });

        if (response.status === 201) {
            pass(testName);
        } else {
            fail(testName, `Status: ${response.status}`);
        }
    } catch (e) {
        fail(testName, e.message);
    }
}

async function testBoardHasCreatedTasks() {
    const testName = 'Board contains created tasks';
    try {
        // Create a unique task
        const uniqueTitle = `Unique_${Date.now()}`;
        const createRes = await request('POST', '/api/tasks', {
            title: uniqueTitle,
            column: 0
        });
        
        if (createRes.status !== 201) {
            fail(testName, 'Could not create task');
            return;
        }
        
        // Get board
        const boardRes = await request('GET', '/api/boards/board-1');
        
        if (boardRes.status === 200 && JSON.stringify(boardRes.data).includes(uniqueTitle)) {
            pass(testName);
        } else {
            fail(testName, 'Task not found in board');
        }
    } catch (e) {
        fail(testName, e.message);
    }
}

async function testColumnEnumValues() {
    const testName = 'Column enum values (0, 1, 2)';
    try {
        // Create task in column 0
        const r0 = await request('POST', '/api/tasks', { title: 'Col0', column: 0 });
        // Create task in column 1
        const r1 = await request('POST', '/api/tasks', { title: 'Col1', column: 1 });
        // Create task in column 2
        const r2 = await request('POST', '/api/tasks', { title: 'Col2', column: 2 });
        
        // Get board and verify columns
        const board = await request('GET', '/api/boards/board-1');
        
        if (r0.status === 201 && r1.status === 201 && r2.status === 201) {
            pass(testName);
        } else {
            fail(testName, `Create statuses: ${r0.status}, ${r1.status}, ${r2.status}`);
        }
    } catch (e) {
        fail(testName, e.message);
    }
}

// Main test runner
async function runTests() {
    console.log('==========================================');
    console.log('Gateway API Tests');
    console.log('==========================================');
    console.log(`Target: http://${API_HOST}:${API_PORT}`);
    console.log('');

    // Check if gateway is running
    try {
        await request('GET', '/api/boards/board-1');
    } catch (e) {
        console.log('\x1b[31mERROR: Gateway not reachable. Make sure it is running on port 8080.\x1b[0m');
        console.log('Start with: cd gateway && node server.js');
        process.exit(1);
    }

    console.log('--- Create Tests ---');
    const taskId = await testCreateTask();
    await testCreateTaskMinimal();
    await testCreateTaskEmptyTitle();
    await testCreateTaskLongContent();
    await testCreateTaskAllColumns();
    await testSpecialCharacters();
    await testUnicodeContent();

    console.log('\n--- Read Tests ---');
    await testGetBoard();
    await testBoardHasCreatedTasks();

    console.log('\n--- Update Tests ---');
    const updateTaskId = await testCreateTask(); // Create fresh task for updates
    await testUpdateTaskTitle(updateTaskId);
    await testUpdateTaskDescription(updateTaskId);
    await testUpdateTaskBoth(updateTaskId);
    await testUpdateNonexistentTask();

    console.log('\n--- Move Tests ---');
    const moveTaskId = await testCreateTask(); // Create fresh task for moves
    await testMoveTask(moveTaskId);
    await testMoveTaskToDone(moveTaskId);
    await testMoveTaskBackToTodo(moveTaskId);

    console.log('\n--- Delete Tests ---');
    const deleteTaskId = await testCreateTask(); // Create fresh task for delete
    await testDeleteTask(deleteTaskId);
    await testDeleteNonexistentTask();

    console.log('\n--- Integration Tests ---');
    await testCreateUpdateDelete();
    await testColumnEnumValues();

    console.log('\n--- Stress Tests ---');
    await testRapidCreates();

    console.log('\n==========================================');
    console.log(`Results: ${testsPass} passed, ${testsFail} failed`);
    console.log('==========================================');

    process.exit(testsFail > 0 ? 1 : 0);
}

runTests();
