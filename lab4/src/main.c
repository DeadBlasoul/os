#include <stddef.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>

#include <rpc.h>
#include <combaseapi.h>

#include "childprocess.h"

#define HEXui "0x%08x"

typedef unsigned int uint;

typedef struct fib_data
{
    size_t ix;
    size_t answer;
} fib_data;

#define BUF_SIZE sizeof(fib_data)

#define COMPUTED             2
#define COMPUTED_RANGE_ERROR SIZE_MAX

size_t computed(const size_t n)
{
    if (n < COMPUTED)
    {
        return n;
    }
    return COMPUTED_RANGE_ERROR;
}

bool fibonacci_recursion(const char* command, fib_data* data)
{
    process child_process;

    const int status = create_process(command, &child_process);
    if (status)
    {
        printf("WinAPI error: " HEXui "\n", (uint)GetLastError());
        return false;
    }
    WaitForSingleObject((HANDLE)child_process.h_process, INFINITE);

    DWORD exit_code;
    GetExitCodeProcess((HANDLE)child_process.h_process, &exit_code);
    if (exit_code != 0)
    {
        return false;
    }

    return true;
}

bool fibonacci(fib_data* data, const char* program)
{
    const size_t ix = data->ix;
    if (ix < COMPUTED)
    {
        data->answer = computed(ix);
        return true;
    }

    // Generate new GUID
    GUID       guid;
    RPC_STATUS status = UuidCreate(&guid);
    if (status != RPC_S_OK && status != RPC_S_UUID_LOCAL_ONLY)
    {
        printf("RPC error: " HEXui "\n", (unsigned int)status);
        return false;
    }

    // Convert GUID to string
    unsigned char* s_guid;
    status = UuidToString(&guid, &s_guid);
    if (status != RPC_S_OK) {
        printf("RPC error: " HEXui "\n", (unsigned int)status);
        return false;
    }

    // Create file mapping
    HANDLE h_map = CreateFileMapping(
        INVALID_HANDLE_VALUE, // use paging file
        NULL,                 // default security
        PAGE_READWRITE,       // read/write access
        0,                    // maximum object size (high-order DWORD)
        BUF_SIZE,             // maximum object size (low-order DWORD)
        (char*)s_guid         // set unique name
    );
    if (h_map == NULL)
    {
        printf("WinAPI error: " HEXui "\n", (uint)GetLastError());
        RpcStringFree(&s_guid);
        return false;
    }

    // Open view of file mapping
    LPVOID p_buf = (LPTSTR)MapViewOfFile(
        h_map,
        FILE_MAP_ALL_ACCESS,
        0,
        0,
        BUF_SIZE
    );
    if (p_buf == NULL)
    {
        printf("WinAPI error: " HEXui "\n", (uint)GetLastError());
        CloseHandle(h_map);
        RpcStringFree(&s_guid);
        return false;
    }

    // Prepare buffers for child process call
    char      command[256];
    size_t    answer = 0;
    fib_data* next   = (fib_data*)p_buf;
    memset(next, 0x00, sizeof(fib_data));
    sprintf_s(command, sizeof(command), "%.200s %.36s", program, (char*)s_guid);

    next->ix = ix - 1;
    bool succeed = fibonacci_recursion(command, next);
    if (succeed)
    {
        answer += next->answer;
        next->ix = ix - 2;
        succeed = fibonacci_recursion(command, next);
        if (succeed)
        {
            answer += next->answer;
            data->answer = answer;
        }
    }

    UnmapViewOfFile(p_buf);
    CloseHandle(h_map);
    RpcStringFree(&s_guid);

    return succeed;
}

int run_as_parent(const char* program_name)
{
    size_t ix;
    if (scanf_s("%zu", &ix) != 1 || ix > 16) {
        printf("Wrong input\n");
        return EXIT_FAILURE;
    }

    fib_data data;
    data.ix = ix;
    if (!fibonacci(&data, program_name)) {
        printf("Unknown error");
        return EXIT_FAILURE;
    }

    char command[128];
    sprintf_s(command, sizeof(command), "wsl echo -n 'What is life? Life is %zu' | wsl cowsay | wsl lolcat", data.answer);
    system(command);
    return EXIT_SUCCESS;
}

int main(const int argc, const char* argv[])
{
    // If argc equals 1 that means we are in the parent process.
    if (argc == 1)
    {
        return run_as_parent(argv[0]);
    }

    // Child process accepts the only one argument.
    if (argc != 2)
    {
        printf("Brown happened! Wrong number of arguments! Stopping...\n");
        return EXIT_FAILURE;
    }

    // Validate GUID string
    GUID             guid;
    const RPC_STATUS status = UuidFromString((RPC_CSTR)argv[1], &guid);
    if (status != RPC_S_OK) {
        printf("RPC error: " HEXui "\n", (unsigned int)status);
        return EXIT_FAILURE;
    }

    // Open file mapping
    HANDLE h_map = OpenFileMapping(
        FILE_MAP_ALL_ACCESS,
        FALSE,
        argv[1]
    );
    if (h_map == NULL)
    {
        printf("WinAPI error: " HEXui "\n", (uint)GetLastError());
        return EXIT_FAILURE;
    }

    // Get virtual memory address
    LPVOID p_buf = MapViewOfFile(
        h_map,
        FILE_MAP_ALL_ACCESS,
        0,
        0,
        BUF_SIZE
    );
    if (p_buf == NULL)
    {
        printf("WinAPI error: " HEXui "\n", (uint)GetLastError());
        CloseHandle(h_map);
        return EXIT_FAILURE;
    }

    fib_data* data = (fib_data*)p_buf;
    fibonacci(data, argv[0]);

    UnmapViewOfFile(p_buf);
    CloseHandle(h_map);

    return EXIT_SUCCESS;
}
