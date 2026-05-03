/*
 * Smart Deadlock Detection and Prevention System
 * resource.c - Resource management implementation
 *
 * Handles dynamic resource allocation, release, and validation.
 */

#include "resource.h"

/*
 * Allocate additional resources to a process.
 * Validates that:
 *   1. The process exists and is active
 *   2. The request doesn't exceed the process's remaining need
 *   3. Sufficient resources are available
 *
 * Returns: 0 on success, -1 on error
 */
int allocate_resources(SystemState *state, int pid, const int *request) {
    int idx, j;

    if (!state || !request) return -1;

    idx = find_process(state, pid);
    if (idx < 0) return -1;

    /* Validate: request must not exceed remaining need */
    for (j = 0; j < state->num_resources; j++) {
        if (request[j] < 0) return -1;
        if (request[j] > state->processes[idx].need[j]) return -1;
    }

    /* Validate: request must not exceed available resources */
    for (j = 0; j < state->num_resources; j++) {
        if (request[j] > state->available[j]) return -1;
    }

    /* Perform allocation */
    for (j = 0; j < state->num_resources; j++) {
        state->processes[idx].allocation[j] += request[j];
        state->available[j] -= request[j];
        state->processes[idx].need[j] -= request[j];
    }

    return 0;
}

/*
 * Release resources from a process back to the available pool.
 * Validates that the release doesn't exceed current allocation.
 *
 * Returns: 0 on success, -1 on error
 */
int release_resources(SystemState *state, int pid, const int *release) {
    int idx, j;

    if (!state || !release) return -1;

    idx = find_process(state, pid);
    if (idx < 0) return -1;

    /* Validate: release must not exceed current allocation */
    for (j = 0; j < state->num_resources; j++) {
        if (release[j] < 0) return -1;
        if (release[j] > state->processes[idx].allocation[j]) return -1;
    }

    /* Perform release */
    for (j = 0; j < state->num_resources; j++) {
        state->processes[idx].allocation[j] -= release[j];
        state->available[j] += release[j];
        state->processes[idx].need[j] += release[j];
    }

    return 0;
}

/*
 * Check if a resource request can be immediately satisfied.
 * Returns: 1 if all request[j] <= available[j], 0 otherwise
 */
int can_satisfy_request(const SystemState *state, const int *request) {
    int j;
    if (!state || !request) return 0;

    for (j = 0; j < state->num_resources; j++) {
        if (request[j] > state->available[j]) return 0;
    }
    return 1;
}

/*
 * Calculate total allocated instances of a specific resource
 * across all active processes.
 */
int get_total_allocated(const SystemState *state, int resource_idx) {
    int i, total = 0;
    if (!state || resource_idx < 0 || resource_idx >= state->num_resources) return 0;

    for (i = 0; i < MAX_PROCESSES; i++) {
        if (state->processes[i].active) {
            total += state->processes[i].allocation[resource_idx];
        }
    }
    return total;
}
