/*
 * Smart Deadlock Detection and Prevention System
 * resource.h - Resource management header
 *
 * Declares functions for dynamic resource allocation, release,
 * and tracking within the simulated OS environment.
 */

#ifndef RESOURCE_H
#define RESOURCE_H

#include "process.h"

/* Allocate additional resources to a process.
 * request[] specifies how many of each resource to add.
 * Returns: 0 on success,
 *         -1 on error (invalid request or request > need)
 *         -2 on error (request > available)
 *         -3 on error (allocation leads to an unsafe state) */
int allocate_resources(SystemState *state, int pid, const int *request);

/* Release resources from a process.
 * release[] specifies how many of each resource to free.
 * Returns: 0 on success, -1 on error */
int release_resources(SystemState *state, int pid, const int *release);

/* Check if a resource request can be satisfied with available resources.
 * Returns: 1 if satisfiable, 0 otherwise */
int can_satisfy_request(const SystemState *state, const int *request);

/* Get the total allocated resources across all processes for resource j */
int get_total_allocated(const SystemState *state, int resource_idx);

#endif /* RESOURCE_H */
