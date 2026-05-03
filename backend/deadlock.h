/*
 * Smart Deadlock Detection and Prevention System
 * deadlock.h - Deadlock algorithms header
 *
 * Declares functions for deadlock detection, prevention,
 * avoidance (Banker's Algorithm), and wait-for graph generation.
 */

#ifndef DEADLOCK_H
#define DEADLOCK_H

#include "process.h"

/* Maximum steps tracked in simulation */
#define MAX_STEPS MAX_PROCESSES

/* Result of Banker's Algorithm execution */
typedef struct {
    int is_safe;                              /* 1 = safe state, 0 = unsafe */
    int safe_sequence[MAX_PROCESSES];          /* Order of safe execution */
    int safe_sequence_length;                  /* Number of processes in sequence */

    /* Step-by-step execution details */
    struct {
        int pid;                              /* Process executed in this step */
        int available_before[MAX_RESOURCES];  /* Available before execution */
        int available_after[MAX_RESOURCES];   /* Available after execution */
        int need[MAX_RESOURCES];              /* Need of this process */
        int allocation[MAX_RESOURCES];        /* Allocation of this process */
    } steps[MAX_STEPS];
    int num_steps;
} BankersResult;

/* Result of deadlock detection */
typedef struct {
    int has_deadlock;                          /* 1 = deadlock detected, 0 = no deadlock */
    int deadlocked_pids[MAX_PROCESSES];       /* PIDs of deadlocked processes */
    int num_deadlocked;                       /* Count of deadlocked processes */
    int completed_pids[MAX_PROCESSES];        /* PIDs that can complete */
    int num_completed;                        /* Count of completable processes */
} DetectionResult;

/* Result of prevention check */
typedef struct {
    int is_valid;                             /* 1 = no prevention violation, 0 = violation */
    char violations[MAX_PROCESSES][256];      /* Description of each violation */
    int num_violations;                       /* Number of violations found */
    char strategy[64];                        /* Prevention strategy name */
} PreventionResult;

/* Wait-for graph edge */
typedef struct {
    int from_pid;                             /* Process waiting */
    int to_pid;                               /* Process holding resource */
    int resource_idx;                         /* Resource being waited on */
} WFGEdge;

/* Wait-for graph result */
typedef struct {
    WFGEdge edges[MAX_PROCESSES * MAX_RESOURCES]; /* Graph edges */
    int num_edges;                                 /* Number of edges */
    int has_cycle;                                 /* 1 = cycle detected */
    int cycle_pids[MAX_PROCESSES];                 /* PIDs in the cycle */
    int cycle_length;                              /* Length of detected cycle */
} WFGResult;

/*
 * Run Banker's Algorithm for deadlock avoidance.
 * Determines if the system is in a safe state and finds a safe sequence.
 */
BankersResult run_bankers(const SystemState *state);

/*
 * Detect deadlock using matrix-based approach.
 * Identifies processes that are deadlocked (cannot complete).
 */
DetectionResult detect_deadlock(const SystemState *state);

/*
 * Check prevention constraints using resource ordering strategy.
 * Validates that processes request resources in increasing order.
 */
PreventionResult check_prevention(const SystemState *state);

/*
 * Build a wait-for graph from the current system state.
 * An edge from P_i to P_j exists if P_i is waiting for a resource
 * that P_j currently holds.
 */
WFGResult build_wait_for_graph(const SystemState *state);

#endif /* DEADLOCK_H */
