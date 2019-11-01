#include "childprocess.h"

#include <Windows.h>
#include <io.h>
#include <fcntl.h>

#ifdef _WIN32
/*
    Here is a WINAPI staff
 */

typedef struct pipe_pair {
    HANDLE hPipeStream_Rd;
    HANDLE hPipeStream_Wr;
} pipe_pair;

int open_rw_pipes(pipe_pair* const in, pipe_pair* const out);
int create_win32_piped_process(const char* const cmd_line, child_process* const cp, pipe_pair* const in, pipe_pair* const out);

int create_child_process(const char* const program, child_process* const cp) {
    int err;

    pipe_pair in;
    pipe_pair out;
    if (err = open_rw_pipes(&in, &out), err != 0) {
        return err;
    }
    if (err = create_win32_piped_process(program, cp, &in, &out), err != 0) {
        return err;
    }

    return 0;
}

int open_rw_pipes(pipe_pair* const in, pipe_pair* const out) {
    HANDLE hChildStd_IN_Rd = NULL;
    HANDLE hChildStd_IN_Wr = NULL;
    HANDLE hChildStd_OUT_Rd = NULL;
    HANDLE hChildStd_OUT_Wr = NULL;
    SECURITY_ATTRIBUTES saAttr;

    saAttr.nLength = sizeof(saAttr);
    saAttr.bInheritHandle = TRUE;
    saAttr.lpSecurityDescriptor = NULL;

    // Create a pipe for the child process STDOUT. 
    if (!CreatePipe(&hChildStd_OUT_Rd, &hChildStd_OUT_Wr, &saAttr, 0)) {
        return 1;
    }

    // Ensure the read handle to the pipe for STDOUT is not inherited.
    if (!SetHandleInformation(hChildStd_OUT_Rd, HANDLE_FLAG_INHERIT, 0)) {
        return 1;
    }

    // Create a pipe for the child process STDIN. 
    if (!CreatePipe(&hChildStd_IN_Rd, &hChildStd_IN_Wr, &saAttr, 0)) {
        return 1;
    }

    // Ensure the write handle to the pipe for STDIN is not inherited. 
    if (!SetHandleInformation(hChildStd_IN_Wr, HANDLE_FLAG_INHERIT, 0)) {
        return 1;
    }

    *in = (pipe_pair){ hChildStd_IN_Rd, hChildStd_IN_Wr };
    *out = (pipe_pair){ hChildStd_OUT_Rd, hChildStd_OUT_Wr };

    return 0;
}

int create_win32_piped_process(const char* const cmd_line, child_process* const cp, pipe_pair* const in, pipe_pair* const out) {
    PROCESS_INFORMATION piProcInfo;
    STARTUPINFO siStartInfo;
    BOOL bSuccess = FALSE;

    // Set up members of the PROCESS_INFORMATION structure. 
    ZeroMemory(&piProcInfo, sizeof(PROCESS_INFORMATION));

    // Set up members of the STARTUPINFO structure. 
    // This structure specifies the STDIN and STDOUT handles for redirection.
    ZeroMemory(&siStartInfo, sizeof(STARTUPINFO));
    siStartInfo.cb = sizeof(STARTUPINFO);
    siStartInfo.hStdError = out->hPipeStream_Wr;
    siStartInfo.hStdOutput = out->hPipeStream_Wr;
    siStartInfo.hStdInput = in->hPipeStream_Rd;
    siStartInfo.dwFlags |= STARTF_USESTDHANDLES;

    // Create the child process. 
    bSuccess = CreateProcess(NULL,
        cmd_line,      // command line 
        NULL,          // process security attributes 
        NULL,          // primary thread security attributes 
        TRUE,          // handles are inherited 
        0,             // creation flags 
        NULL,          // use environment of parent
        NULL,          // use current directory 
        &siStartInfo,  // STARTUPINFO pointer 
        &piProcInfo);  // receives PROCESS_INFORMATION 

     // If an error occurs, exit the application. 
    if (!bSuccess) {
        return 1;
    }

    cp->inp = _fdopen(_open_osfhandle(in->hPipeStream_Wr, _O_TEXT), "w");
    cp->out = _fdopen(_open_osfhandle(out->hPipeStream_Rd, _O_TEXT), "r");
    cp->process_handle = (system_handle*)piProcInfo.hProcess;
    cp->thread_handle = (system_handle*)piProcInfo.hThread;
    return 0;
}

#endif // _WIN32
