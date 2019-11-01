#pragma once

#include <stdio.h>

typedef struct system_handle system_handle;

typedef struct child_process {
    FILE* inp;  // stdin
    FILE* out;  // stdout

    system_handle* process_handle;
#ifdef _WIN32
    system_handle* thread_handle;
#endif // _WIN32
} child_process;

int create_child_process(const char* const program, child_process* const cp);
