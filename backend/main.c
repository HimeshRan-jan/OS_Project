/*
 * Smart Deadlock Detection and Prevention System
 * main.c - JSON Command Dispatcher
 *
 * Reads a JSON command from stdin, dispatches to the appropriate
 * algorithm, and writes the JSON result to stdout.
 *
 * Supported commands:
 *   "get_state"      - Return current system state
 *   "add_process"    - Add a new process
 *   "delete_process" - Remove a process
 *   "allocate"       - Allocate resources to a process
 *   "release"        - Release resources from a process
 *   "bankers"        - Run Banker's Algorithm
 *   "detect"         - Run deadlock detection
 *   "prevent"        - Run prevention check
 *   "wfg"            - Build wait-for graph
 *
 * JSON I/O is handled with a minimal custom parser to avoid
 * external dependencies.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdarg.h>
#include "process.h"
#include "resource.h"
#include "deadlock.h"

/* ============================================================
 * MINIMAL JSON PARSER / GENERATOR
 * Tailored for this project's specific data shapes.
 * ============================================================ */

#define JSON_BUF_SIZE 65536
#define MAX_TOKEN_LEN 1024

/* Skip whitespace in JSON string */
static const char *skip_ws(const char *s) {
    while (*s && isspace((unsigned char)*s)) s++;
    return s;
}

/* Parse a JSON string value (between quotes). Returns pointer past closing quote. */
static const char *parse_string(const char *s, char *out, int max_len) {
    int i = 0;
    s = skip_ws(s);
    if (*s != '"') return NULL;
    s++; /* skip opening quote */
    while (*s && *s != '"' && i < max_len - 1) {
        if (*s == '\\') {
            s++;
            if (*s == '"') out[i++] = '"';
            else if (*s == '\\') out[i++] = '\\';
            else if (*s == 'n') out[i++] = '\n';
            else if (*s == 't') out[i++] = '\t';
            else out[i++] = *s;
        } else {
            out[i++] = *s;
        }
        s++;
    }
    out[i] = '\0';
    if (*s == '"') s++; /* skip closing quote */
    return s;
}

/* Parse a JSON integer. Returns pointer past the number. */
static const char *parse_int(const char *s, int *out) {
    s = skip_ws(s);
    *out = 0;
    int neg = 0;
    if (*s == '-') { neg = 1; s++; }
    while (*s >= '0' && *s <= '9') {
        *out = (*out) * 10 + (*s - '0');
        s++;
    }
    if (neg) *out = -(*out);
    return s;
}

/* Parse a JSON array of integers like [1, 2, 3]. Returns pointer past ']'. */
static const char *parse_int_array(const char *s, int *arr, int *count, int max_count) {
    *count = 0;
    s = skip_ws(s);
    if (*s != '[') return NULL;
    s++; /* skip '[' */
    s = skip_ws(s);
    if (*s == ']') { s++; return s; } /* empty array */

    while (*count < max_count) {
        s = parse_int(s, &arr[*count]);
        (*count)++;
        s = skip_ws(s);
        if (*s == ',') { s++; s = skip_ws(s); }
        else if (*s == ']') { s++; break; }
        else break;
    }
    return s;
}

/* Find a key in a JSON object. Returns pointer to the value after ':'. */
static const char *find_key(const char *json, const char *key) {
    char search_key[256];
    const char *p = json;

    snprintf(search_key, sizeof(search_key), "\"%s\"", key);

    while ((p = strstr(p, search_key)) != NULL) {
        p += strlen(search_key);
        p = skip_ws(p);
        if (*p == ':') {
            p++;
            p = skip_ws(p);
            return p;
        }
    }
    return NULL;
}

/* Read a string value for a given key */
static int read_string_key(const char *json, const char *key, char *out, int max_len) {
    const char *val = find_key(json, key);
    if (!val) return 0;
    parse_string(val, out, max_len);
    return 1;
}

/* Read an integer value for a given key */
static int read_int_key(const char *json, const char *key, int *out) {
    const char *val = find_key(json, key);
    if (!val) return 0;
    parse_int(val, out);
    return 1;
}

/* Read an integer array for a given key */
static int read_int_array_key(const char *json, const char *key, int *arr, int *count, int max) {
    const char *val = find_key(json, key);
    if (!val) return 0;
    parse_int_array(val, arr, count, max);
    return 1;
}

/* ============================================================
 * JSON OUTPUT HELPERS
 * ============================================================ */

static char output_buf[JSON_BUF_SIZE];
static int out_pos = 0;

static void out_reset(void) { out_pos = 0; output_buf[0] = '\0'; }

static void out_append(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    out_pos += vsnprintf(output_buf + out_pos, JSON_BUF_SIZE - out_pos, fmt, args);
    va_end(args);
}

static void out_int_array(const int *arr, int count) {
    int i;
    out_append("[");
    for (i = 0; i < count; i++) {
        out_append("%d", arr[i]);
        if (i < count - 1) out_append(",");
    }
    out_append("]");
}

/* ============================================================
 * PARSE SYSTEM STATE FROM JSON
 * ============================================================ */

/*
 * Parse the full system state from the "processes" array in JSON.
 * Expected format:
 * {
 *   "num_resources": 3,
 *   "total_resources": [10, 5, 7],
 *   "processes": [
 *     {"pid": 0, "allocation": [0,1,0], "max_need": [7,5,3]},
 *     ...
 *   ]
 * }
 */
static int parse_system_state(const char *json, SystemState *state) {
    int num_res = 0, total[MAX_RESOURCES], total_count = 0;
    const char *procs, *p;

    if (!read_int_key(json, "num_resources", &num_res) || num_res <= 0 || num_res > MAX_RESOURCES) {
        return -1;
    }

    if (!read_int_array_key(json, "total_resources", total, &total_count, MAX_RESOURCES)) {
        return -1;
    }

    init_system(state, num_res, total);

    /* Parse processes array */
    procs = find_key(json, "processes");
    if (!procs) return 0; /* No processes yet, just initialized state */

    p = skip_ws(procs);
    if (*p != '[') return -1;
    p++; /* skip '[' */
    p = skip_ws(p);

    while (*p && *p != ']') {
        if (*p == '{') {
            /* Find the closing brace for this process object */
            const char *obj_start = p;
            int depth = 1;
            p++;
            while (*p && depth > 0) {
                if (*p == '{') depth++;
                else if (*p == '}') depth--;
                p++;
            }

            /* Extract process data from the object substring */
            int obj_len = (int)(p - obj_start);
            char obj_buf[4096];
            if (obj_len < (int)sizeof(obj_buf)) {
                strncpy(obj_buf, obj_start, obj_len);
                obj_buf[obj_len] = '\0';

                int pid = -1, alloc[MAX_RESOURCES], max_n[MAX_RESOURCES];
                int alloc_count = 0, max_count = 0;

                read_int_key(obj_buf, "pid", &pid);
                read_int_array_key(obj_buf, "allocation", alloc, &alloc_count, MAX_RESOURCES);
                read_int_array_key(obj_buf, "max_need", max_n, &max_count, MAX_RESOURCES);

                if (pid >= 0 && alloc_count > 0 && max_count > 0) {
                    add_process(state, pid, alloc, max_n);
                }
            }

            p = skip_ws(p);
            if (*p == ',') p++;
            p = skip_ws(p);
        } else {
            p++;
        }
    }

    return 0;
}

/* ============================================================
 * OUTPUT SYSTEM STATE AS JSON
 * ============================================================ */
static void output_state(const SystemState *state) {
    int i, first;

    out_append("\"state\":{");
    out_append("\"num_resources\":%d,", state->num_resources);
    out_append("\"num_processes\":%d,", state->num_processes);

    out_append("\"total_resources\":");
    out_int_array(state->total_resources, state->num_resources);
    out_append(",");

    out_append("\"available\":");
    out_int_array(state->available, state->num_resources);
    out_append(",");

    out_append("\"processes\":[");
    first = 1;
    for (i = 0; i < MAX_PROCESSES; i++) {
        if (!state->processes[i].active) continue;
        if (!first) out_append(",");
        first = 0;

        out_append("{\"pid\":%d,", state->processes[i].pid);
        out_append("\"allocation\":");
        out_int_array(state->processes[i].allocation, state->num_resources);
        out_append(",\"max_need\":");
        out_int_array(state->processes[i].max_need, state->num_resources);
        out_append(",\"need\":");
        out_int_array(state->processes[i].need, state->num_resources);
        out_append("}");
    }
    out_append("]}");
}

/* ============================================================
 * COMMAND HANDLERS
 * ============================================================ */

static void handle_bankers(const SystemState *state) {
    BankersResult r = run_bankers(state);
    int i;

    out_append("{\"success\":true,\"command\":\"bankers\",");
    out_append("\"is_safe\":%s,", r.is_safe ? "true" : "false");

    out_append("\"safe_sequence\":");
    out_int_array(r.safe_sequence, r.safe_sequence_length);
    out_append(",");

    out_append("\"steps\":[");
    for (i = 0; i < r.num_steps; i++) {
        if (i > 0) out_append(",");
        out_append("{\"pid\":%d,", r.steps[i].pid);
        out_append("\"available_before\":");
        out_int_array(r.steps[i].available_before, state->num_resources);
        out_append(",\"available_after\":");
        out_int_array(r.steps[i].available_after, state->num_resources);
        out_append(",\"need\":");
        out_int_array(r.steps[i].need, state->num_resources);
        out_append(",\"allocation\":");
        out_int_array(r.steps[i].allocation, state->num_resources);
        out_append("}");
    }
    out_append("],");

    output_state(state);
    out_append("}");
}

static void handle_detect(const SystemState *state) {
    DetectionResult r = detect_deadlock(state);

    out_append("{\"success\":true,\"command\":\"detect\",");
    out_append("\"has_deadlock\":%s,", r.has_deadlock ? "true" : "false");

    out_append("\"deadlocked_pids\":");
    out_int_array(r.deadlocked_pids, r.num_deadlocked);
    out_append(",");

    out_append("\"completed_pids\":");
    out_int_array(r.completed_pids, r.num_completed);
    out_append(",");

    /* Also include WFG data */
    WFGResult wfg = build_wait_for_graph(state);
    out_append("\"wfg\":{\"num_edges\":%d,\"has_cycle\":%s,",
        wfg.num_edges, wfg.has_cycle ? "true" : "false");

    out_append("\"edges\":[");
    for (int i = 0; i < wfg.num_edges; i++) {
        if (i > 0) out_append(",");
        out_append("{\"from\":%d,\"to\":%d,\"resource\":%d}",
            wfg.edges[i].from_pid, wfg.edges[i].to_pid, wfg.edges[i].resource_idx);
    }
    out_append("],");

    out_append("\"cycle_pids\":");
    out_int_array(wfg.cycle_pids, wfg.cycle_length);
    out_append("},");

    output_state(state);
    out_append("}");
}

static void handle_prevent(const SystemState *state) {
    PreventionResult r = check_prevention(state);

    out_append("{\"success\":true,\"command\":\"prevent\",");
    out_append("\"is_valid\":%s,", r.is_valid ? "true" : "false");
    out_append("\"strategy\":\"%s\",", r.strategy);

    out_append("\"violations\":[");
    for (int i = 0; i < r.num_violations; i++) {
        if (i > 0) out_append(",");
        out_append("\"%s\"", r.violations[i]);
    }
    out_append("],");

    output_state(state);
    out_append("}");
}

static void handle_get_state(const SystemState *state) {
    out_append("{\"success\":true,\"command\":\"get_state\",");
    output_state(state);
    out_append("}");
}

static void handle_add_process(const char *json, SystemState *state) {
    int pid = -1, alloc[MAX_RESOURCES], max_n[MAX_RESOURCES];
    int alloc_count = 0, max_count = 0;

    /* Read the new process data from the "new_process" object */
    const char *np = find_key(json, "new_process");
    if (!np) {
        out_append("{\"success\":false,\"error\":\"Missing new_process field\"}");
        return;
    }

    /* Find the object boundaries */
    const char *obj_start = skip_ws(np);
    if (*obj_start != '{') {
        out_append("{\"success\":false,\"error\":\"new_process must be an object\"}");
        return;
    }

    /* Extract object substring */
    const char *p = obj_start;
    int depth = 1;
    p++;
    while (*p && depth > 0) {
        if (*p == '{') depth++;
        else if (*p == '}') depth--;
        p++;
    }

    int obj_len = (int)(p - obj_start);
    char obj_buf[4096];
    if (obj_len >= (int)sizeof(obj_buf)) {
        out_append("{\"success\":false,\"error\":\"Process data too large\"}");
        return;
    }
    strncpy(obj_buf, obj_start, obj_len);
    obj_buf[obj_len] = '\0';

    read_int_key(obj_buf, "pid", &pid);
    read_int_array_key(obj_buf, "allocation", alloc, &alloc_count, MAX_RESOURCES);
    read_int_array_key(obj_buf, "max_need", max_n, &max_count, MAX_RESOURCES);

    if (pid < 0) {
        out_append("{\"success\":false,\"error\":\"Invalid or missing pid\"}");
        return;
    }

    int result = add_process(state, pid, alloc, max_n);
    if (result < 0) {
        out_append("{\"success\":false,\"error\":\"Failed to add process. Check PID uniqueness and resource values.\"}");
        return;
    }

    out_append("{\"success\":true,\"command\":\"add_process\",\"pid\":%d,", pid);
    output_state(state);
    out_append("}");
}

static void handle_delete_process(const char *json, SystemState *state) {
    int pid = -1;
    read_int_key(json, "pid", &pid);

    if (pid < 0) {
        out_append("{\"success\":false,\"error\":\"Invalid or missing pid\"}");
        return;
    }

    int result = delete_process(state, pid);
    if (result < 0) {
        out_append("{\"success\":false,\"error\":\"Process not found\"}");
        return;
    }

    out_append("{\"success\":true,\"command\":\"delete_process\",\"pid\":%d,", pid);
    output_state(state);
    out_append("}");
}

static void handle_allocate(const char *json, SystemState *state) {
    int pid = -1, request[MAX_RESOURCES], req_count = 0;

    read_int_key(json, "pid", &pid);
    read_int_array_key(json, "request", request, &req_count, MAX_RESOURCES);

    if (pid < 0) {
        out_append("{\"success\":false,\"error\":\"Invalid or missing pid\"}");
        return;
    }

    int result = allocate_resources(state, pid, request);
    if (result == -1) {
        out_append("{\"success\":false,\"error\":\"Allocation failed. Invalid process or request exceeds remaining need.\"}");
        return;
    } else if (result == -2) {
        out_append("{\"success\":false,\"error\":\"Allocation failed. Request exceeds currently available resources.\"}");
        return;
    } else if (result == -3) {
        out_append("{\"success\":false,\"error\":\"Allocation denied. Granting this request would lead to an unsafe state (potential deadlock).\"}");
        return;
    } else if (result < 0) {
        out_append("{\"success\":false,\"error\":\"Allocation failed for an unknown reason.\"}");
        return;
    }

    out_append("{\"success\":true,\"command\":\"allocate\",\"pid\":%d,", pid);
    output_state(state);
    out_append("}");
}

static void handle_release(const char *json, SystemState *state) {
    int pid = -1, release[MAX_RESOURCES], rel_count = 0;

    read_int_key(json, "pid", &pid);
    read_int_array_key(json, "release", release, &rel_count, MAX_RESOURCES);

    if (pid < 0) {
        out_append("{\"success\":false,\"error\":\"Invalid or missing pid\"}");
        return;
    }

    int result = release_resources(state, pid, release);
    if (result < 0) {
        out_append("{\"success\":false,\"error\":\"Release failed. Cannot release more than allocated.\"}");
        return;
    }

    out_append("{\"success\":true,\"command\":\"release\",\"pid\":%d,", pid);
    output_state(state);
    out_append("}");
}

/* ============================================================
 * MAIN — Read JSON from stdin, dispatch command, write result
 * ============================================================ */


int main(void) {
    char input[JSON_BUF_SIZE];
    char command[64];
    SystemState state;
    int bytes_read = 0;
    int ch;

    /* Read all of stdin into input buffer */
    while ((ch = fgetc(stdin)) != EOF && bytes_read < JSON_BUF_SIZE - 1) {
        input[bytes_read++] = (char)ch;
    }
    input[bytes_read] = '\0';

    if (bytes_read == 0) {
        fprintf(stdout, "{\"success\":false,\"error\":\"Empty input\"}");
        return 0;
    }

    /* Parse the system state from input */
    if (parse_system_state(input, &state) < 0) {
        fprintf(stdout, "{\"success\":false,\"error\":\"Failed to parse system state\"}");
        return 0;
    }

    /* Read the command */
    if (!read_string_key(input, "command", command, sizeof(command))) {
        fprintf(stdout, "{\"success\":false,\"error\":\"Missing command field\"}");
        return 0;
    }

    /* Dispatch to appropriate handler */
    out_reset();

    if (strcmp(command, "bankers") == 0) {
        handle_bankers(&state);
    } else if (strcmp(command, "detect") == 0) {
        handle_detect(&state);
    } else if (strcmp(command, "prevent") == 0) {
        handle_prevent(&state);
    } else if (strcmp(command, "get_state") == 0) {
        handle_get_state(&state);
    } else if (strcmp(command, "add_process") == 0) {
        handle_add_process(input, &state);
    } else if (strcmp(command, "delete_process") == 0) {
        handle_delete_process(input, &state);
    } else if (strcmp(command, "allocate") == 0) {
        handle_allocate(input, &state);
    } else if (strcmp(command, "release") == 0) {
        handle_release(input, &state);
    } else {
        out_append("{\"success\":false,\"error\":\"Unknown command: %s\"}", command);
    }

    /* Write output to stdout */
    fprintf(stdout, "%s", output_buf);
    fflush(stdout);

    return 0;
}
