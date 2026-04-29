/*
 * Smart Deadlock Detection and Prevention System
 * server.js - Node.js REST API Bridge
 *
 * This Express server acts as a bridge between the HTML/JS frontend
 * and the compiled C backend. Each API call:
 *   1. Receives JSON from the frontend
 *   2. Spawns the C executable
 *   3. Pipes the JSON to the C program via stdin
 *   4. Reads the C program's JSON response from stdout
 *   5. Returns the response to the frontend
 */

const express = require('express');
const cors = require('cors');
const { execFile } = require('child_process');
const path = require('path');
const fs = require('fs');

const app = express();
const PORT = 3000;

/* Middleware */
app.use(cors());
app.use(express.json({ limit: '1mb' }));

/* Serve static frontend files */
app.use(express.static(path.join(__dirname, '..', 'frontend')));

/* Path to the compiled C executable */
const C_EXECUTABLE = path.join(__dirname, '..', 'backend',
    process.platform === 'win32' ? 'deadlock_system.exe' : 'deadlock_system');

/*
 * Execute the C backend with a JSON command.
 * Spawns the process, sends JSON via stdin, collects stdout.
 */
function runCBackend(jsonInput) {
    return new Promise((resolve, reject) => {
        /* Check if executable exists */
        if (!fs.existsSync(C_EXECUTABLE)) {
            reject(new Error(
                `C executable not found at ${C_EXECUTABLE}. ` +
                'Please compile the backend first: cd backend && gcc -Wall -Wextra -O2 -o deadlock_system main.c process.c resource.c deadlock.c'
            ));
            return;
        }

        const child = require('child_process').spawn(C_EXECUTABLE, [], {
            stdio: ['pipe', 'pipe', 'pipe']
        });

        let stdout = '';
        let stderr = '';

        child.stdout.on('data', (data) => { stdout += data.toString(); });
        child.stderr.on('data', (data) => { stderr += data.toString(); });

        child.on('close', (code) => {
            if (code !== 0) {
                reject(new Error(`C program exited with code ${code}: ${stderr}`));
                return;
            }
            try {
                const result = JSON.parse(stdout);
                resolve(result);
            } catch (e) {
                reject(new Error(`Invalid JSON from C program: ${stdout}`));
            }
        });

        child.on('error', (err) => {
            reject(new Error(`Failed to spawn C program: ${err.message}`));
        });

        /* Send JSON input to stdin and close */
        child.stdin.write(jsonInput);
        child.stdin.end();

        /* Timeout after 10 seconds */
        setTimeout(() => {
            child.kill();
            reject(new Error('C program timed out'));
        }, 10000);
    });
}

/*
 * Generic API handler that adds the command field
 * and forwards to the C backend.
 */
async function handleCommand(req, res, command) {
    try {
        const input = { ...req.body, command };
        const inputJson = JSON.stringify(input);

        console.log(`[${new Date().toISOString()}] ${command.toUpperCase()} request`);

        const result = await runCBackend(inputJson);
        res.json(result);
    } catch (error) {
        console.error(`Error in ${command}:`, error.message);
        res.status(500).json({
            success: false,
            error: error.message
        });
    }
}

/* ============================================================
 * REST API ENDPOINTS
 * ============================================================ */

/* Get current system state */
app.post('/api/state', (req, res) => handleCommand(req, res, 'get_state'));

/* Run Banker's Algorithm */
app.post('/api/bankers', (req, res) => handleCommand(req, res, 'bankers'));

/* Run Deadlock Detection */
app.post('/api/detect', (req, res) => handleCommand(req, res, 'detect'));

/* Run Prevention Check */
app.post('/api/prevent', (req, res) => handleCommand(req, res, 'prevent'));

/* Add a process */
app.post('/api/process/add', (req, res) => handleCommand(req, res, 'add_process'));

/* Delete a process */
app.post('/api/process/delete', (req, res) => handleCommand(req, res, 'delete_process'));

/* Allocate resources */
app.post('/api/resource/allocate', (req, res) => handleCommand(req, res, 'allocate'));

/* Release resources */
app.post('/api/resource/release', (req, res) => handleCommand(req, res, 'release'));

/* Health check */
app.get('/api/health', (req, res) => {
    const exeExists = fs.existsSync(C_EXECUTABLE);
    res.json({
        status: 'ok',
        backend_compiled: exeExists,
        executable_path: C_EXECUTABLE,
        timestamp: new Date().toISOString()
    });
});

/* Serve the main page */
app.get('/', (req, res) => {
    res.sendFile(path.join(__dirname, '..', 'frontend', 'index.html'));
});

/* Start the server */
app.listen(PORT, () => {
    console.log(`\n╔══════════════════════════════════════════════════════╗`);
    console.log(`║  Smart Deadlock Detection & Prevention System       ║`);
    console.log(`║  Server running at http://localhost:${PORT}             ║`);
    console.log(`╚══════════════════════════════════════════════════════╝\n`);

    if (!fs.existsSync(C_EXECUTABLE)) {
        console.log('⚠  WARNING: C backend executable not found!');
        console.log('   Compile it first:');
        console.log('   cd backend && gcc -Wall -Wextra -O2 -o deadlock_system main.c process.c resource.c deadlock.c\n');
    } else {
        console.log('✓  C backend executable found');
    }
});
