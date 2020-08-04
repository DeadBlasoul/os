#include <tasking/launcher.hpp>

#ifdef _WIN32
/*
 * Windows implementation
 */
#include <windows.h>

#include <stdexcept>
#include <string>

namespace {
    /// Actual WinAPI task handlers
    struct task_handlers {
        HANDLE h_process;
        HANDLE h_thread;

        auto close_safe() -> void;
    };

    /// Explicit const HANDLE
    using const_handle = const HANDLE;

    /// Function to close WinAPI handles.
    /*!
     *  We may use NULL handles which is prohibited by WinAPI,
     *  so this function checks it before call of actual CloseHandle.
     *
     * @param handle: const HANDLE that will be closed; can be NULL/nullptr
     */
    auto close_handle_safe(const_handle handle) -> BOOL {
        if (handle != nullptr) {
            return CloseHandle(handle);
        }
        return TRUE;
    }

    auto task_handlers::close_safe() -> void {
        close_handle_safe(h_process);
        close_handle_safe(h_thread);
        std::memset(this, 0x00, sizeof(task_handlers));
    }
}

tasking::task::~task() noexcept {
    //
    // Here we just close task handles no matter it's running or already done.
    //
    auto const& handlers = this->as<task_handlers>();
    close_handle_safe(handlers.h_process);
    close_handle_safe(handlers.h_thread);
}

auto tasking::task::wait() -> void {
    //
    // Handler can be closed only using destructor; we can safely invoke next call.
    //
    WaitForSingleObject(this->as<task_handlers>().h_process, INFINITE);
}

auto tasking::task::kill() -> void {
    auto& handlers = this->as<task_handlers>();

    TerminateProcess(handlers.h_process, 1);
    handlers.close_safe();
}

auto tasking::launcher::start() const noexcept(false) -> task {
    auto                task = tasking::task {};
    STARTUPINFOA        info = {sizeof(info)};
    PROCESS_INFORMATION process_info;
    auto const args = task_info_.path.string() + " " + std::string{ task_info_.args };

    //
    // Create new process
    //
    auto const status = CreateProcessA(
        task_info_.path.string().c_str(),
        const_cast<LPSTR>(args.data()),
        nullptr,
        nullptr,
        FALSE,
        CREATE_NEW_CONSOLE,
        nullptr,
        nullptr,
        &info,
        &process_info);
    if (!status) {
        throw std::runtime_error {"unable to start new task"};
    }

    //
    // Update task handlers
    //
    task.as<task_handlers>() = {
        .h_process = process_info.hProcess,
        .h_thread = process_info.hThread,
    };

    return task;
}

#endif
