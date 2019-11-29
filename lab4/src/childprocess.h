#pragma once

#include <stdio.h>

typedef struct system_handle system_handle;

typedef struct process {
    FILE* inp;  // stdin
    FILE* out;  // stdout

    system_handle* h_process;
#ifdef _WIN32
    system_handle* h_thread;
#endif // _WIN32
} process;

int create_process(const char* const program, process* const cp);
