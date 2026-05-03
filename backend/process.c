/*
 * Smart Deadlock Detection and Prevention System
 * process.c - Process management implementation
 *
 * Handles creation, deletion, and management of simulated processes.
 */

#include "process.h"

/*
 * Initialize a new system state with given resource configuration.
 * Sets all process slots to inactive and configures resource totals.
 */
void init_system(SystemState *state, int num_resources, const int *total) {
    int i, j;

    if (!state || num_resources <= 0 || num_resources > MAX_RESOURCES) return;

    state->num_processes = 0;
    state->num_resources = num_resources;

    /* Initialize all process slots as inactive */
    for (i = 0; i < MAX_PROCESSES; i++) {
        state->processes[i].active = 0;
        state->processes[i].pid = -1;
        for (j = 0; j < MAX_RESOURCES; j++) {
            state->processes[i].allocation[j] = 0;
            state->processes[i].max_need[j] = 0;
            state->processes[i].need[j] = 0;
        }
    }

    /* Set total and available resources */
    for (i = 0; i < num_resources; i++) {
        state->total_resources[i] = total[i];
        state->available[i] = total[i];
    }
}

/*
 * Add a new process to the system.
 * Validates that allocation doesn't exceed max_need and that
 * sufficient resources are available.
 *
 * Returns: 0 on success, -1 on validation failure
 */
int add_process(SystemState *state, int pid, const int *allocation, const int *max_need) {
    int i, slot = -1;

    if (!state || !allocation || !max_need) return -1;
    if (state->num_processes >= MAX_PROCESSES) return -1;

    /* Check if PID already exists */
    if (find_process(state, pid) >= 0) return -1;

    /* Find first available slot */
    for (i = 0; i < MAX_PROCESSES; i++) {
        if (!state->processes[i].active) {
            slot = i;
            break;
        }
    }
    if (slot < 0) return -1;

    /* Validate: allocation must not exceed max_need */
    for (i = 0; i < state->num_resources; i++) {
        if (allocation[i] < 0 || max_need[i] < 0) return -1;
        if (allocation[i] > max_need[i]) return -1;
    }

    /* Set process data */
    state->processes[slot].pid = pid;
    state->processes[slot].active = 1;
    for (i = 0; i < state->num_resources; i++) {
        state->processes[slot].allocation[i] = allocation[i];
        state->processes[slot].max_need[i] = max_need[i];
        state->processes[slot].need[i] = max_need[i] - allocation[i];
    }

    state->num_processes++;
    recalculate_available(state);
    return 0;
}

/*
 * Remove a process from the system and release its resources.
 * Returns: 0 on success, -1 if process not found
 */
int delete_process(SystemState *state, int pid) {
    int idx, i;

    if (!state) return -1;

    idx = find_process(state, pid);
    if (idx < 0) return -1;

    /* Release allocated resources back to available pool */
    state->processes[idx].active = 0;
    state->processes[idx].pid = -1;
    for (i = 0; i < state->num_resources; i++) {
        state->processes[idx].allocation[i] = 0;
        state->processes[idx].max_need[i] = 0;
        state->processes[idx].need[i] = 0;
    }

    state->num_processes--;
    recalculate_available(state);
    return 0;
}

/*
 * Find the index of a process by its PID.
 * Returns: index in processes array, or -1 if not found
 */
int find_process(const SystemState *state, int pid) {
    int i;
    if (!state) return -1;

    for (i = 0; i < MAX_PROCESSES; i++) {
        if (state->processes[i].active && state->processes[i].pid == pid) {
            return i;
        }
    }
    return -1;
}

/*
 * Recalculate the available resources vector.
 * available[j] = total[j] - sum(allocation[i][j]) for all active processes
 */
void recalculate_available(SystemState *state) {
    int i, j;
    if (!state) return;

    for (j = 0; j < state->num_resources; j++) {
        int used = 0;
        for (i = 0; i < MAX_PROCESSES; i++) {
            if (state->processes[i].active) {
                used += state->processes[i].allocation[j];
            }
        }
        state->available[j] = state->total_resources[j] - used;
    }
}

/*
 * Recalculate the need matrix for all active processes.
 * need[i][j] = max_need[i][j] - allocation[i][j]
 */
void recalculate_need(SystemState *state) {
    int i, j;
    if (!state) return;

    for (i = 0; i < MAX_PROCESSES; i++) {
        if (state->processes[i].active) {
            for (j = 0; j < state->num_resources; j++) {
                state->processes[i].need[j] =
                    state->processes[i].max_need[j] - state->processes[i].allocation[j];
            }
        }
    }
}

/*
 * Validate a process's resource state.
 * Checks: allocation <= max_need, and all values are non-negative.
 * Returns: 1 if valid, 0 if invalid
 */
int validate_process(const SystemState *state, int pid) {
    int idx, j;

    if (!state) return 0;

    idx = find_process(state, pid);
    if (idx < 0) return 0;

    for (j = 0; j < state->num_resources; j++) {
        if (state->processes[idx].allocation[j] < 0) return 0;
        if (state->processes[idx].max_need[j] < 0) return 0;
        if (state->processes[idx].allocation[j] > state->processes[idx].max_need[j]) return 0;
    }
    return 1;
}
