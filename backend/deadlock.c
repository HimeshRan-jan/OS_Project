/*
 * Smart Deadlock Detection and Prevention System
 * deadlock.c - Deadlock algorithms implementation
 *
 * Implements:
 *   1. Banker's Algorithm (Deadlock Avoidance)
 *   2. Matrix-based Deadlock Detection
 *   3. Resource Ordering Prevention
 *   4. Wait-For Graph Construction with Cycle Detection
 */

#include "deadlock.h"

/*
 * =============================================================
 * BANKER'S ALGORITHM — Deadlock Avoidance
 * =============================================================
 *
 * The Banker's Algorithm determines if the system is in a safe
 * state by attempting to find a safe execution sequence.
 *
 * Algorithm:
 *   1. Initialize work[] = available[], finish[] = false
 *   2. Find a process i where finish[i]==false and need[i] <= work
 *   3. If found: work += allocation[i], finish[i] = true, record step
 *   4. Repeat until no more processes can be found
 *   5. If all finish[i]==true → SAFE, else → UNSAFE
 */
BankersResult run_bankers(const SystemState *state) {
    BankersResult result;
    int work[MAX_RESOURCES];
    int finish[MAX_PROCESSES];
    int i, j, found, count;
    int num_active = 0;

    /* Initialize result */
    memset(&result, 0, sizeof(BankersResult));

    if (!state || state->num_processes == 0) {
        result.is_safe = 1; /* Empty system is trivially safe */
        return result;
    }

    /* Initialize work vector with available resources */
    for (j = 0; j < state->num_resources; j++) {
        work[j] = state->available[j];
    }

    /* Initialize finish flags */
    for (i = 0; i < MAX_PROCESSES; i++) {
        finish[i] = !state->processes[i].active; /* Inactive = already finished */
        if (state->processes[i].active) num_active++;
    }

    count = 0;

    /* Main loop: repeatedly find a process that can complete */
    do {
        found = 0;
        for (i = 0; i < MAX_PROCESSES; i++) {
            if (finish[i]) continue;

            /* Check if need[i] <= work */
            int can_run = 1;
            for (j = 0; j < state->num_resources; j++) {
                if (state->processes[i].need[j] > work[j]) {
                    can_run = 0;
                    break;
                }
            }

            if (can_run) {
                /* Record step details */
                result.steps[count].pid = state->processes[i].pid;
                for (j = 0; j < state->num_resources; j++) {
                    result.steps[count].available_before[j] = work[j];
                    result.steps[count].need[j] = state->processes[i].need[j];
                    result.steps[count].allocation[j] = state->processes[i].allocation[j];
                }

                /* Simulate process completion: work += allocation[i] */
                for (j = 0; j < state->num_resources; j++) {
                    work[j] += state->processes[i].allocation[j];
                    result.steps[count].available_after[j] = work[j];
                }

                /* Mark process as finished */
                finish[i] = 1;
                result.safe_sequence[count] = state->processes[i].pid;
                count++;
                found = 1;
            }
        }
    } while (found);

    result.safe_sequence_length = count;
    result.num_steps = count;
    result.is_safe = (count == num_active) ? 1 : 0;

    return result;
}

/*
 * =============================================================
 * DEADLOCK DETECTION — Matrix-based Approach
 * =============================================================
 *
 * Similar to Banker's but uses current requests instead of max needs.
 * A process is deadlocked if it cannot complete even after all
 * non-deadlocked processes release their resources.
 *
 * Algorithm:
 *   1. Initialize work[] = available[], finish[] = false
 *   2. Find process i where finish[i]==false and need[i] <= work
 *   3. If found: work += allocation[i], finish[i] = true
 *   4. Repeat until no more can be found
 *   5. Any process with finish[i]==false is deadlocked
 */
DetectionResult detect_deadlock(const SystemState *state) {
    DetectionResult result;
    int work[MAX_RESOURCES];
    int finish[MAX_PROCESSES];
    int i, j, found;

    memset(&result, 0, sizeof(DetectionResult));

    if (!state || state->num_processes == 0) {
        result.has_deadlock = 0;
        return result;
    }

    /* Initialize work = available */
    for (j = 0; j < state->num_resources; j++) {
        work[j] = state->available[j];
    }

    /* Initialize finish: processes with zero allocation can finish trivially */
    for (i = 0; i < MAX_PROCESSES; i++) {
        if (!state->processes[i].active) {
            finish[i] = 1;
            continue;
        }
        /* Check if allocation is all zeros */
        int all_zero = 1;
        for (j = 0; j < state->num_resources; j++) {
            if (state->processes[i].allocation[j] != 0) {
                all_zero = 0;
                break;
            }
        }
        finish[i] = all_zero;
    }

    /* Main detection loop */
    do {
        found = 0;
        for (i = 0; i < MAX_PROCESSES; i++) {
            if (finish[i] || !state->processes[i].active) continue;

            /* Check if need[i] <= work */
            int can_complete = 1;
            for (j = 0; j < state->num_resources; j++) {
                if (state->processes[i].need[j] > work[j]) {
                    can_complete = 0;
                    break;
                }
            }

            if (can_complete) {
                /* Process can complete, release its resources */
                for (j = 0; j < state->num_resources; j++) {
                    work[j] += state->processes[i].allocation[j];
                }
                finish[i] = 1;
                result.completed_pids[result.num_completed++] = state->processes[i].pid;
                found = 1;
            }
        }
    } while (found);

    /* Any unfinished active process is deadlocked */
    for (i = 0; i < MAX_PROCESSES; i++) {
        if (state->processes[i].active && !finish[i]) {
            result.deadlocked_pids[result.num_deadlocked++] = state->processes[i].pid;
            result.has_deadlock = 1;
        }
    }

    return result;
}

/*
 * =============================================================
 * DEADLOCK PREVENTION — Resource Ordering Strategy
 * =============================================================
 *
 * Enforces that processes request resources in strictly increasing
 * order of resource index. This eliminates circular wait.
 *
 * For each process, the highest-index resource currently allocated
 * must be less than the lowest-index resource still needed.
 * If not, a prevention violation is flagged.
 */
PreventionResult check_prevention(const SystemState *state) {
    PreventionResult result;
    int i, j;

    memset(&result, 0, sizeof(PreventionResult));
    strcpy(result.strategy, "Resource Ordering");

    if (!state || state->num_processes == 0) {
        result.is_valid = 1;
        return result;
    }

    result.is_valid = 1;

    for (i = 0; i < MAX_PROCESSES; i++) {
        if (!state->processes[i].active) continue;

        /* Find the highest index resource currently allocated */
        int highest_allocated = -1;
        for (j = state->num_resources - 1; j >= 0; j--) {
            if (state->processes[i].allocation[j] > 0) {
                highest_allocated = j;
                break;
            }
        }

        /* Find the lowest index resource still needed */
        int lowest_needed = -1;
        for (j = 0; j < state->num_resources; j++) {
            if (state->processes[i].need[j] > 0) {
                lowest_needed = j;
                break;
            }
        }

        /* Check ordering: highest allocated should be < lowest needed */
        if (highest_allocated >= 0 && lowest_needed >= 0) {
            if (highest_allocated >= lowest_needed) {
                result.is_valid = 0;
                snprintf(result.violations[result.num_violations], 256,
                    "P%d: holds R%d but needs R%d (violates ordering R%d < R%d)",
                    state->processes[i].pid, highest_allocated, lowest_needed,
                    lowest_needed, highest_allocated);
                result.num_violations++;
            }
        }
    }

    return result;
}

/*
 * =============================================================
 * WAIT-FOR GRAPH — Construction and Cycle Detection
 * =============================================================
 *
 * Builds a directed graph where an edge from P_i to P_j means
 * P_i is waiting for a resource currently held by P_j.
 *
 * A process P_i is considered waiting for resource R_k if need[i][k] > 0
 * and available[k] == 0 (resource is fully allocated).
 * The edge points to any P_j where allocation[j][k] > 0.
 *
 * Cycle detection uses DFS with coloring:
 *   WHITE(0) = unvisited, GRAY(1) = in current path, BLACK(2) = done
 */

/* DFS helper for cycle detection */
static int dfs_cycle(int adj[MAX_PROCESSES][MAX_PROCESSES], int n,
                     int node, int color[], int parent_map[],
                     int cycle_path[], int *cycle_len,
                     int pid_map[]) {
    int i;
    color[node] = 1; /* GRAY - in current DFS path */

    for (i = 0; i < n; i++) {
        if (!adj[node][i]) continue;

        if (color[i] == 1) {
            /* Found a cycle! Trace back */
            int curr = node;
            *cycle_len = 0;
            cycle_path[(*cycle_len)++] = pid_map[i];
            while (curr != i && *cycle_len < MAX_PROCESSES) {
                cycle_path[(*cycle_len)++] = pid_map[curr];
                curr = parent_map[curr];
            }
            return 1;
        }

        if (color[i] == 0) {
            parent_map[i] = node;
            if (dfs_cycle(adj, n, i, color, parent_map, cycle_path, cycle_len, pid_map))
                return 1;
        }
    }

    color[node] = 2; /* BLACK - done */
    return 0;
}

WFGResult build_wait_for_graph(const SystemState *state) {
    WFGResult result;
    int adj[MAX_PROCESSES][MAX_PROCESSES]; /* Adjacency matrix for active processes */
    int pid_map[MAX_PROCESSES]; /* Map from compact index to PID */
    int idx_map[MAX_PROCESSES]; /* Map from compact index to state->processes index */
    int n = 0; /* Number of active processes */
    int i, j, k;
    int color[MAX_PROCESSES];
    int parent_map[MAX_PROCESSES];

    memset(&result, 0, sizeof(WFGResult));
    memset(adj, 0, sizeof(adj));

    if (!state || state->num_processes == 0) return result;

    /* Build compact index for active processes */
    for (i = 0; i < MAX_PROCESSES; i++) {
        if (state->processes[i].active) {
            pid_map[n] = state->processes[i].pid;
            idx_map[n] = i;
            n++;
        }
    }

    /* Build wait-for edges */
    for (i = 0; i < n; i++) {
        int pi = idx_map[i];
        for (k = 0; k < state->num_resources; k++) {
            /* Process i needs resource k and it's not available */
            if (state->processes[pi].need[k] > 0 && state->available[k] == 0) {
                /* Find who holds resource k */
                for (j = 0; j < n; j++) {
                    if (i == j) continue;
                    int pj = idx_map[j];
                    if (state->processes[pj].allocation[k] > 0) {
                        /* Edge: i waits for j because of resource k */
                        if (!adj[i][j]) { /* Avoid duplicate edges in adjacency */
                            adj[i][j] = 1;
                        }
                        /* Always record the detailed edge */
                        if (result.num_edges < MAX_PROCESSES * MAX_RESOURCES) {
                            result.edges[result.num_edges].from_pid = pid_map[i];
                            result.edges[result.num_edges].to_pid = pid_map[j];
                            result.edges[result.num_edges].resource_idx = k;
                            result.num_edges++;
                        }
                    }
                }
            }
        }
    }

    /* Cycle detection using DFS */
    memset(color, 0, sizeof(color));
    memset(parent_map, -1, sizeof(parent_map));

    for (i = 0; i < n; i++) {
        if (color[i] == 0) {
            if (dfs_cycle(adj, n, i, color, parent_map,
                          result.cycle_pids, &result.cycle_length, pid_map)) {
                result.has_cycle = 1;
                break;
            }
        }
    }

    return result;
}
