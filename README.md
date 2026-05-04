# Smart Deadlock Detection & Prevention System

A professional-grade Operating Systems project that simulates deadlock detection, prevention, and avoidance using a modular C backend, Node.js REST API bridge, and an interactive web dashboard.

## Architecture

```
Frontend (HTML/CSS/JS)  ←→  Node.js Bridge (Express)  ←→  C Backend (gcc compiled)
         REST API                stdin/stdout JSON
```

## Features

- **Banker's Algorithm** — Deadlock avoidance with safe sequence computation
- **Deadlock Detection** — Matrix-based detection identifying deadlocked processes
- **Deadlock Prevention** — Resource ordering strategy validation
- **Wait-For Graph** — Visual graph with cycle detection
- **Step-by-Step Simulation** — Animated execution trace
- **3 Sample Scenarios** — Safe state, deadlock, and prevention violation
- **JSON Export** — Download results for analysis

## Prerequisites

- **GCC** (MinGW-w64 on Windows) — for compiling C code
- **Node.js** (v14+) — for the REST API bridge

## Quick Start

### 1. Compile the C Backend

```bash
cd backend
gcc -Wall -Wextra -O2 -o deadlock_system main.c process.c resource.c deadlock.c
```

On Windows, you can also run `build.bat`.

### 2. Install Node.js Dependencies

```bash
cd server
npm install
```

### 3. Start the Server

```bash
cd server
npm start
```

### 4. Open the Dashboard

Navigate to `http://localhost:3000` in your browser.

## Project Structure

```
OS project/
├── backend/
│   ├── main.c          # JSON command dispatcher
│   ├── process.h/c     # Process management
│   ├── resource.h/c    # Resource allocation
│   ├── deadlock.h/c    # Detection, prevention, Banker's, WFG
│   ├── Makefile         # Build configuration
│   └── build.bat        # Windows build script
├── server/
│   ├── server.js        # Express REST API bridge
│   └── package.json
├── frontend/
│   ├── index.html       # Dashboard layout
│   ├── style.css        # Dark theme styling
│   └── app.js           # Frontend logic
└── README.md
```

## Sample Input/Output

### Safe State (Banker's Algorithm)

**Input:** 5 processes, 3 resource types, Total = [10, 5, 7]

| PID | Allocation | Max Need |
|-----|-----------|----------|
| P0  | 0, 1, 0   | 7, 5, 3  |
| P1  | 2, 0, 0   | 3, 2, 2  |
| P2  | 3, 0, 2   | 9, 0, 2  |
| P3  | 2, 1, 1   | 2, 2, 2  |
| P4  | 0, 0, 2   | 4, 3, 3  |

**Output:** Safe State ✓, Sequence: P1 → P3 → P4 → P0 → P2

### Deadlock State

**Input:** 3 processes, 3 resources, Total = [3, 3, 2]

| PID | Allocation | Max Need |
|-----|-----------|----------|
| P0  | 1, 1, 0   | 2, 2, 1  |
| P1  | 1, 1, 1   | 2, 2, 2  |
| P2  | 1, 1, 1   | 3, 3, 2  |

**Output:** Deadlock Detected ⚠ — All processes deadlocked

## API Endpoints

| Method | Endpoint | Description |
|--------|----------|-------------|
| POST | `/api/bankers` | Run Banker's Algorithm |
| POST | `/api/detect` | Run deadlock detection |
| POST | `/api/prevent` | Run prevention check |
| POST | `/api/process/add` | Add a process |
| POST | `/api/process/delete` | Delete a process |
| POST | `/api/resource/allocate` | Allocate resources |
| POST | `/api/resource/release` | Release resources |
| GET  | `/api/health` | Health check |
