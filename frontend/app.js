/*
 * Smart Deadlock Detection & Prevention System
 * app.js — Frontend logic: state management, API calls, rendering
 */

const API = 'http://localhost:3000/api';

/* ---- Application State ---- */
const appState = {
    numResources: 0,
    totalResources: [],
    processes: [],
    available: [],
    initialized: false,
    lastResult: null,
    simSteps: [],
    simIndex: 0,
    simTimer: null
};

/* ---- Sample Test Cases ---- */
const SAMPLES = {
    safe: {
        numResources: 3, total: [10, 5, 7],
        processes: [
            { pid: 0, allocation: [0,1,0], max_need: [7,5,3] },
            { pid: 1, allocation: [2,0,0], max_need: [3,2,2] },
            { pid: 2, allocation: [3,0,2], max_need: [9,0,2] },
            { pid: 3, allocation: [2,1,1], max_need: [2,2,2] },
            { pid: 4, allocation: [0,0,2], max_need: [4,3,3] }
        ]
    },
    deadlock: {
        numResources: 3, total: [3, 3, 2],
        processes: [
            { pid: 0, allocation: [1,1,0], max_need: [2,2,1] },
            { pid: 1, allocation: [1,1,1], max_need: [2,2,2] },
            { pid: 2, allocation: [1,1,1], max_need: [3,3,2] }
        ]
    },
    prevention: {
        numResources: 3, total: [10, 5, 7],
        processes: [
            { pid: 0, allocation: [0,1,2], max_need: [7,5,3] },
            { pid: 1, allocation: [3,0,0], max_need: [3,2,2] },
            { pid: 2, allocation: [1,1,0], max_need: [9,2,2] }
        ]
    }
};

/* ---- Utility Functions ---- */
function parseCSV(str) {
    return str.split(',').map(s => parseInt(s.trim(), 10)).filter(n => !isNaN(n));
}
function timeStr() {
    const d = new Date();
    return d.toTimeString().slice(0, 8);
}
function $(id) { return document.getElementById(id); }

/* ---- Logging ---- */
function addLog(msg, type = 'info') {
    const c = $('log-container');
    const e = document.createElement('div');
    e.className = `log-entry log-${type}`;
    e.innerHTML = `<span class="log-time">${timeStr()}</span><span class="log-msg">${msg}</span>`;
    c.appendChild(e);
    c.scrollTop = c.scrollHeight;
}

/* ---- API Communication ---- */
async function apiCall(endpoint, data) {
    const payload = {
        num_resources: appState.numResources,
        total_resources: appState.totalResources,
        processes: appState.processes,
        ...data
    };
    try {
        const res = await fetch(`${API}/${endpoint}`, {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify(payload)
        });
        const json = await res.json();
        if (json.state) {
            appState.available = json.state.available || [];
            appState.processes = json.state.processes || [];
        }
        return json;
    } catch (err) {
        addLog(`API Error: ${err.message}`, 'error');
        return { success: false, error: err.message };
    }
}

/* ---- System Status Indicator ---- */
function setStatus(text, type) {
    const el = $('system-status');
    el.className = `status-indicator ${type || ''}`;
    el.querySelector('.status-text').textContent = text;
}

/* ---- Initialize System ---- */
function initSystem(numRes, total, procs) {
    appState.numResources = numRes;
    appState.totalResources = [...total];
    appState.available = [...total];
    appState.initialized = true;

    /* Deep copy processes and compute need if missing */
    if (procs && procs.length > 0) {
        appState.processes = procs.map(p => {
            const proc = { ...p, allocation: [...p.allocation], max_need: [...p.max_need] };
            /* Compute need = max_need - allocation */
            proc.need = proc.max_need.map((m, j) => m - proc.allocation[j]);
            return proc;
        });
        for (let j = 0; j < numRes; j++) {
            let used = 0;
            appState.processes.forEach(p => { used += p.allocation[j]; });
            appState.available[j] = total[j] - used;
        }
    } else {
        appState.processes = [];
    }

    renderResources();
    renderProcesses();
    enableAlgoButtons(appState.processes.length > 0);
    setStatus('Ready', '');
    addLog(`System initialized: ${numRes} resource types, Total=[${total.join(',')}]`, 'success');
    if (appState.processes.length > 0) {
        addLog(`Loaded ${appState.processes.length} processes`, 'info');
    }
}

/* ---- Render Resource Bars ---- */
function renderResources() {
    const c = $('resource-bars');
    $('badge-resource-count').textContent = `${appState.numResources} types`;
    if (!appState.initialized) { c.innerHTML = '<div class="empty-state">Initialize system to see resources</div>'; return; }

    let html = '';
    for (let j = 0; j < appState.numResources; j++) {
        const total = appState.totalResources[j];
        const avail = appState.available[j];
        const used = total - avail;
        const pct = total > 0 ? (used / total * 100) : 0;
        const color = pct > 85 ? 'var(--accent-red)' : pct > 60 ? 'var(--accent-amber)' : '';
        html += `<div class="resource-bar-item">
            <span class="resource-bar-label">R${j}</span>
            <div class="resource-bar-track"><div class="resource-bar-fill" style="width:${pct}%;${color ? `background:${color}` : ''}"></div></div>
            <span class="resource-bar-text">${used}/${total} used</span>
        </div>`;
    }
    c.innerHTML = html;
}

/* ---- Render Process Tables ---- */
function renderProcesses() {
    const c = $('matrix-tables');
    const procs = appState.processes;
    $('badge-process-count').textContent = `${procs.length} processes`;

    if (procs.length === 0) {
        c.innerHTML = `<div class="empty-state" id="empty-processes">
            <svg width="48" height="48" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1" opacity="0.3"><path d="M17 21v-2a4 4 0 00-4-4H5a4 4 0 00-4 4v2"/><circle cx="9" cy="7" r="4"/></svg>
            <p>No processes yet. Click "Add Process" to begin.</p></div>`;
        enableAlgoButtons(false);
        return;
    }

    const nR = appState.numResources;
    const hdr = Array.from({length: nR}, (_, i) => `<th>R${i}</th>`).join('');

    let html = '<div class="matrix-container"><table class="matrix-table"><thead><tr><th>PID</th>';
    // Allocation headers
    for (let i = 0; i < nR; i++) html += `<th>Alloc R${i}</th>`;
    for (let i = 0; i < nR; i++) html += `<th>Max R${i}</th>`;
    for (let i = 0; i < nR; i++) html += `<th>Need R${i}</th>`;
    html += '<th></th></tr></thead><tbody>';

    procs.forEach(p => {
        html += `<tr><td class="pid-cell">P${p.pid}</td>`;
        p.allocation.slice(0, nR).forEach(v => { html += `<td>${v}</td>`; });
        p.max_need.slice(0, nR).forEach(v => { html += `<td>${v}</td>`; });
        p.need.slice(0, nR).forEach(v => { html += `<td>${v}</td>`; });
        html += `<td class="delete-cell"><button class="delete-btn" onclick="deleteProcess(${p.pid})" title="Delete P${p.pid}">✕</button></td></tr>`;
    });

    html += '</tbody></table></div>';

    // Available row
    html += `<div style="margin-top:0.75rem;font-size:0.78rem;color:var(--text-secondary)"><strong>Available:</strong> <span style="font-family:var(--font-mono);color:var(--accent-cyan)">[${appState.available.slice(0, nR).join(', ')}]</span></div>`;

    c.innerHTML = html;
    enableAlgoButtons(true);
}

function enableAlgoButtons(enabled) {
    $('btn-detect').disabled = !enabled;
    $('btn-bankers').disabled = !enabled;
    $('btn-prevent').disabled = !enabled;
}

/* ---- Add Process ---- */
async function addProcess() {
    const pid = parseInt($('input-pid').value, 10);
    const alloc = parseCSV($('input-allocation').value);
    const maxN = parseCSV($('input-max-need').value);

    if (isNaN(pid) || pid < 0) { addLog('Invalid PID', 'error'); return; }
    if (alloc.length !== appState.numResources) { addLog(`Allocation must have ${appState.numResources} values`, 'error'); return; }
    if (maxN.length !== appState.numResources) { addLog(`Max Need must have ${appState.numResources} values`, 'error'); return; }
    for (let j = 0; j < appState.numResources; j++) {
        if (alloc[j] > maxN[j]) { addLog(`Allocation[${j}] exceeds Max Need[${j}]`, 'error'); return; }
    }

    const result = await apiCall('process/add', {
        new_process: { pid, allocation: alloc, max_need: maxN }
    });

    if (result.success) {
        addLog(`Process P${pid} added successfully`, 'success');
        renderResources();
        renderProcesses();
        $('add-process-form').style.display = 'none';
        $('input-pid').value = parseInt($('input-pid').value, 10) + 1;
    } else {
        addLog(`Failed to add P${pid}: ${result.error}`, 'error');
    }
}

/* ---- Delete Process ---- */
async function deleteProcess(pid) {
    const result = await apiCall('process/delete', { pid });
    if (result.success) {
        addLog(`Process P${pid} deleted`, 'warn');
        renderResources();
        renderProcesses();
    } else {
        addLog(`Failed to delete P${pid}: ${result.error}`, 'error');
    }
}

/* ---- Run Banker's Algorithm ---- */
async function runBankers() {
    addLog('Running Banker\'s Algorithm...', 'info');
    setStatus('Processing', 'warning');

    const result = await apiCall('bankers', {});
    appState.lastResult = result;

    if (!result.success) { addLog(`Banker's failed: ${result.error}`, 'error'); setStatus('Error', 'danger'); return; }

    const card = $('card-result');
    card.style.display = 'block';
    card.className = `card card-result ${result.is_safe ? 'result-safe' : 'result-danger'}`;

    $('result-title').textContent = "Banker's Algorithm Result";
    const badge = $('result-badge');

    if (result.is_safe) {
        badge.className = 'badge badge-safe';
        badge.textContent = '✓ SAFE STATE';
        setStatus('Safe', '');
        addLog(`Safe state confirmed. Sequence: ${result.safe_sequence.map(p => 'P' + p).join(' → ')}`, 'success');
    } else {
        badge.className = 'badge badge-danger';
        badge.textContent = '✗ UNSAFE STATE';
        setStatus('Unsafe', 'danger');
        addLog('System is in UNSAFE state — no safe sequence exists', 'error');
    }

    let html = '';
    if (result.is_safe) {
        html += '<div class="result-section"><h3>Safe Sequence</h3><div class="safe-sequence">';
        result.safe_sequence.forEach((pid, i) => {
            if (i > 0) html += '<span class="seq-arrow">→</span>';
            html += `<span class="seq-item" style="animation-delay:${i * 0.1}s">P${pid}</span>`;
        });
        html += '</div></div>';
    } else {
        html += '<div class="result-section"><h3>Status</h3><div class="deadlock-list"><span class="deadlock-item">No safe execution sequence found</span></div></div>';
    }

    $('result-content').innerHTML = html;

    // Show simulation steps
    if (result.steps && result.steps.length > 0) {
        appState.simSteps = result.steps;
        appState.simIndex = 0;
        showSimulation();
    }
}

/* ---- Run Deadlock Detection ---- */
async function runDetection() {
    addLog('Running deadlock detection...', 'info');
    setStatus('Processing', 'warning');

    const result = await apiCall('detect', {});
    appState.lastResult = result;

    if (!result.success) { addLog(`Detection failed: ${result.error}`, 'error'); setStatus('Error', 'danger'); return; }

    const card = $('card-result');
    card.style.display = 'block';

    $('result-title').textContent = 'Deadlock Detection Result';
    const badge = $('result-badge');

    if (result.has_deadlock) {
        card.className = 'card card-result result-danger';
        badge.className = 'badge badge-danger';
        badge.textContent = '⚠ DEADLOCK DETECTED';
        setStatus('Deadlock!', 'danger');
        addLog(`DEADLOCK detected! Processes: ${result.deadlocked_pids.map(p => 'P' + p).join(', ')}`, 'error');
    } else {
        card.className = 'card card-result result-safe';
        badge.className = 'badge badge-safe';
        badge.textContent = '✓ NO DEADLOCK';
        setStatus('No Deadlock', '');
        addLog('No deadlock detected — all processes can complete', 'success');
    }

    let html = '';
    if (result.has_deadlock) {
        html += '<div class="result-section"><h3>Deadlocked Processes</h3><div class="deadlock-list">';
        result.deadlocked_pids.forEach(pid => { html += `<span class="deadlock-item">P${pid}</span>`; });
        html += '</div></div>';
    }
    if (result.completed_pids && result.completed_pids.length > 0) {
        html += '<div class="result-section"><h3>Completable Processes</h3><div class="safe-sequence">';
        result.completed_pids.forEach(pid => { html += `<span class="seq-item">P${pid}</span>`; });
        html += '</div></div>';
    }

    $('result-content').innerHTML = html;

    // Draw WFG
    if (result.wfg && result.wfg.edges && result.wfg.edges.length > 0) {
        $('card-graph').style.display = 'block';
        drawWFG(result.wfg);
    } else {
        $('card-graph').style.display = 'none';
    }
}

/* ---- Run Prevention Check ---- */
async function runPrevention() {
    addLog('Running prevention check (Resource Ordering)...', 'info');
    setStatus('Processing', 'warning');

    const result = await apiCall('prevent', {});
    appState.lastResult = result;

    if (!result.success) { addLog(`Prevention check failed: ${result.error}`, 'error'); setStatus('Error', 'danger'); return; }

    const card = $('card-result');
    card.style.display = 'block';

    $('result-title').textContent = 'Prevention Check — ' + (result.strategy || 'Resource Ordering');
    const badge = $('result-badge');

    if (result.is_valid) {
        card.className = 'card card-result result-safe';
        badge.className = 'badge badge-safe';
        badge.textContent = '✓ VALID';
        setStatus('Valid', '');
        addLog('No prevention violations — resource ordering is maintained', 'success');
    } else {
        card.className = 'card card-result result-warning';
        badge.className = 'badge badge-warn';
        badge.textContent = '⚠ VIOLATIONS';
        setStatus('Violations', 'warning');
        addLog(`${result.num_violations || result.violations.length} prevention violation(s) found`, 'warn');
    }

    let html = '';
    if (!result.is_valid && result.violations) {
        html += '<div class="result-section"><h3>Violations</h3><ul class="violation-list">';
        result.violations.forEach(v => { html += `<li>${v}</li>`; });
        html += '</ul></div>';
    } else {
        html += '<div class="result-section"><p style="color:var(--accent-green)">All processes request resources in valid increasing order. No circular wait possible.</p></div>';
    }

    $('result-content').innerHTML = html;
}

/* ---- Simulation Step-by-Step ---- */
function showSimulation() {
    const sec = $('section-simulation');
    sec.style.display = 'block';
    $('btn-sim-prev').disabled = true;
    $('btn-sim-next').disabled = appState.simSteps.length <= 1;
    renderSimStep();
}

function renderSimStep() {
    const steps = appState.simSteps;
    const idx = appState.simIndex;
    $('step-counter').textContent = `Step ${idx + 1} / ${steps.length}`;
    $('btn-sim-prev').disabled = idx === 0;
    $('btn-sim-next').disabled = idx >= steps.length - 1;

    const step = steps[idx];
    const nR = appState.numResources;

    let html = `<div class="sim-step">
        <div class="sim-step-header">
            <div class="sim-step-num">${idx + 1}</div>
            <div class="sim-step-title">Execute Process P${step.pid}</div>
        </div>
        <div class="sim-step-detail">
            <div>Need: [${step.need.slice(0, nR).join(', ')}]</div>
            <div>Available Before: [${step.available_before.slice(0, nR).join(', ')}]</div>
            <div>Allocation Released: [${step.allocation.slice(0, nR).join(', ')}]</div>
            <div>Available After: [${step.available_after.slice(0, nR).join(', ')}]</div>
        </div>
    </div>`;

    $('simulation-content').innerHTML = html;
}

/* ---- Wait-For Graph Drawing (Canvas) ---- */
function drawWFG(wfg) {
    const canvas = $('wfg-canvas');
    const ctx = canvas.getContext('2d');
    const dpr = window.devicePixelRatio || 1;

    canvas.width = canvas.clientWidth * dpr;
    canvas.height = canvas.clientHeight * dpr;
    ctx.scale(dpr, dpr);

    const W = canvas.clientWidth;
    const H = canvas.clientHeight;

    ctx.clearRect(0, 0, W, H);

    // Collect unique PIDs
    const pids = new Set();
    wfg.edges.forEach(e => { pids.add(e.from); pids.add(e.to); });
    const pidArr = [...pids].sort((a, b) => a - b);
    const n = pidArr.length;
    if (n === 0) return;

    // Position nodes in a circle
    const cx = W / 2, cy = H / 2, radius = Math.min(W, H) * 0.32;
    const positions = {};
    pidArr.forEach((pid, i) => {
        const angle = (2 * Math.PI * i / n) - Math.PI / 2;
        positions[pid] = { x: cx + radius * Math.cos(angle), y: cy + radius * Math.sin(angle) };
    });

    // Cycle PIDs set for coloring
    const cyclePids = new Set(wfg.cycle_pids || []);

    // Draw edges
    wfg.edges.forEach(e => {
        const from = positions[e.from], to = positions[e.to];
        if (!from || !to) return;

        const isCycleEdge = cyclePids.has(e.from) && cyclePids.has(e.to);
        ctx.beginPath();
        ctx.strokeStyle = isCycleEdge ? '#ef4444' : 'rgba(0,229,255,0.4)';
        ctx.lineWidth = isCycleEdge ? 2.5 : 1.5;

        // Offset to avoid overlap with node
        const dx = to.x - from.x, dy = to.y - from.y;
        const dist = Math.sqrt(dx * dx + dy * dy);
        const nx = dx / dist, ny = dy / dist;
        const r = 20;

        ctx.moveTo(from.x + nx * r, from.y + ny * r);
        ctx.lineTo(to.x - nx * r, to.y - ny * r);
        ctx.stroke();

        // Arrowhead
        const ax = to.x - nx * r, ay = to.y - ny * r;
        const aSize = 8;
        ctx.beginPath();
        ctx.fillStyle = isCycleEdge ? '#ef4444' : 'rgba(0,229,255,0.6)';
        ctx.moveTo(ax, ay);
        ctx.lineTo(ax - aSize * nx + aSize * 0.4 * ny, ay - aSize * ny - aSize * 0.4 * nx);
        ctx.lineTo(ax - aSize * nx - aSize * 0.4 * ny, ay - aSize * ny + aSize * 0.4 * nx);
        ctx.fill();

        // Resource label on edge
        const mx = (from.x + to.x) / 2, my = (from.y + to.y) / 2;
        ctx.font = '10px Inter, sans-serif';
        ctx.fillStyle = 'rgba(148,163,184,0.7)';
        ctx.fillText(`R${e.resource}`, mx + 5, my - 5);
    });

    // Draw nodes
    pidArr.forEach(pid => {
        const pos = positions[pid];
        const inCycle = cyclePids.has(pid);

        ctx.beginPath();
        ctx.arc(pos.x, pos.y, 18, 0, Math.PI * 2);
        ctx.fillStyle = inCycle ? 'rgba(239,68,68,0.2)' : 'rgba(0,229,255,0.1)';
        ctx.fill();
        ctx.strokeStyle = inCycle ? '#ef4444' : '#00e5ff';
        ctx.lineWidth = 2;
        ctx.stroke();

        ctx.font = 'bold 12px Inter, sans-serif';
        ctx.fillStyle = inCycle ? '#ef4444' : '#00e5ff';
        ctx.textAlign = 'center';
        ctx.textBaseline = 'middle';
        ctx.fillText(`P${pid}`, pos.x, pos.y);
    });
}

/* ---- Export Results ---- */
function exportResults() {
    const data = {
        timestamp: new Date().toISOString(),
        system: {
            numResources: appState.numResources,
            totalResources: appState.totalResources,
            available: appState.available,
            processes: appState.processes
        },
        lastResult: appState.lastResult
    };

    const blob = new Blob([JSON.stringify(data, null, 2)], { type: 'application/json' });
    const url = URL.createObjectURL(blob);
    const a = document.createElement('a');
    a.href = url;
    a.download = `deadlock_results_${Date.now()}.json`;
    a.click();
    URL.revokeObjectURL(url);
    addLog('Results exported as JSON', 'success');
}

/* ---- Event Listeners ---- */
document.addEventListener('DOMContentLoaded', () => {
    // Init System
    $('btn-init-system').addEventListener('click', () => {
        const numRes = parseInt($('input-num-resources').value, 10);
        const total = parseCSV($('input-total-resources').value);
        if (numRes <= 0 || numRes > 10) { addLog('Resource types must be 1-10', 'error'); return; }
        if (total.length !== numRes) { addLog(`Expected ${numRes} values, got ${total.length}`, 'error'); return; }
        if (total.some(v => v < 0)) { addLog('Resource values must be non-negative', 'error'); return; }
        initSystem(numRes, total, []);
    });

    // Load Sample toggle
    $('btn-load-sample').addEventListener('click', () => {
        const sel = $('sample-selector');
        sel.style.display = sel.style.display === 'none' ? 'flex' : 'none';
    });

    // Sample buttons
    document.querySelectorAll('.sample-btn').forEach(btn => {
        btn.addEventListener('click', () => {
            const s = SAMPLES[btn.dataset.sample];
            $('input-num-resources').value = s.numResources;
            $('input-total-resources').value = s.total.join(', ');
            initSystem(s.numResources, s.total, s.processes);
            $('sample-selector').style.display = 'none';
        });
    });

    // Add Process
    $('btn-add-process').addEventListener('click', () => {
        if (!appState.initialized) { addLog('Initialize system first', 'error'); return; }
        const form = $('add-process-form');
        form.style.display = form.style.display === 'none' ? 'block' : 'none';
    });
    $('btn-confirm-add').addEventListener('click', addProcess);
    $('btn-cancel-add').addEventListener('click', () => { $('add-process-form').style.display = 'none'; });

    // Algorithms
    $('btn-bankers').addEventListener('click', runBankers);
    $('btn-detect').addEventListener('click', runDetection);
    $('btn-prevent').addEventListener('click', runPrevention);

    // Simulation controls
    $('btn-sim-prev').addEventListener('click', () => {
        if (appState.simIndex > 0) { appState.simIndex--; renderSimStep(); }
    });
    $('btn-sim-next').addEventListener('click', () => {
        if (appState.simIndex < appState.simSteps.length - 1) { appState.simIndex++; renderSimStep(); }
    });
    $('btn-sim-auto').addEventListener('click', () => {
        if (appState.simTimer) { clearInterval(appState.simTimer); appState.simTimer = null; $('btn-sim-auto').textContent = '▶ Auto Play'; return; }
        appState.simIndex = 0;
        $('btn-sim-auto').textContent = '⏸ Pause';
        appState.simTimer = setInterval(() => {
            renderSimStep();
            appState.simIndex++;
            if (appState.simIndex >= appState.simSteps.length) {
                clearInterval(appState.simTimer); appState.simTimer = null;
                appState.simIndex = appState.simSteps.length - 1;
                $('btn-sim-auto').textContent = '▶ Auto Play';
            }
        }, 1200);
    });

    // Export & Clear
    $('btn-export').addEventListener('click', exportResults);
    $('btn-clear-log').addEventListener('click', () => {
        $('log-container').innerHTML = '';
        addLog('Log cleared', 'info');
    });
});
