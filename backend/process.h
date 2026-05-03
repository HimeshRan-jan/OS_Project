/*
 * Smart Deadlock Detection and Prevention System
 * process.h - Process management header
 *
 * Defines process structures and management functions for the
 * simulated OS environment.
 */

#ifndef PROCESS_H
#define PROCESS_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* System limits */
#define MAX_PROCESSES 20
#define MAX_RESOURCES 10

/* Process structure representing a single OS process */
typedef struct {
    int pid;                          /* Unique process identifier */
    int allocation[MAX_RESOURCES];    /* Currently allocated resources */
    int max_need[MAX_RESOURCES];      /* Maximum resource needs */
    int need[MAX_RESOURCES];          /* Remaining needs (max - allocation) */
    int active;                       /* 1 if process is active, 0 otherwise */
} Process;

/* System state containing all processes and resource information */
typedef struct {
    Process processes[MAX_PROCESSES];
    int num_processes;
    int num_resources;
    int total_resources[MAX_RESOURCES];
    int available[MAX_RESOURCES];
} SystemState;

/* Initialize a new system state */
void init_system(SystemState *state, int num_resources, const int *total);

/* Add a process to the system. Returns 0 on success, -1 on error */
int add_process(SystemState *state, int pid, const int *allocation, const int *max_need);

/* Remove a process from the system. Returns 0 on success, -1 on error */
int delete_process(SystemState *state, int pid);

/* Find process index by PID. Returns -1 if not found */
int find_process(const SystemState *state, int pid);

/* Recalculate the available resources based on allocations */
void recalculate_available(SystemState *state);

/* Recalculate the need matrix for all processes */
void recalculate_need(SystemState *state);

/* Validate that a process's allocation does not exceed max_need or available */
int validate_process(const SystemState *state, int pid);

#endif /* PROCESS_H */
