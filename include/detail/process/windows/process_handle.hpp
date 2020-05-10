#ifndef DETAIL_PROCESS_WINDOWS_CHILD_HPP
#define DETAIL_PROCESS_WINDOWS_CHILD_HPP

#include <asio/windows/object_handle.hpp>
#include <optional>

namespace PROCESS_NAMESPACE::detail::process::windows {

typedef int pid_t;

constexpr auto still_active = STILL_ACTIVE;
constexpr inline int eval_exit_status(int in ) {return in;}

struct process_handle
{
    ::PROCESS_INFORMATION proc_info{nullptr, nullptr, 0,0};

    explicit process_handle(const ::PROCESS_INFORMATION &pi) :
                                  proc_info(pi)
    {}

    explicit process_handle(pid_t pid) :
                                  proc_info{nullptr, nullptr, 0,0}
    {
        auto h = ::OpenProcess(
                PROCESS_ALL_ACCESS,
                static_cast<BOOL>(0),
                 pid);

        if (h == nullptr)
            throw_last_error("OpenProcess() failed");
        proc_info.hProcess = h;
        proc_info.dwProcessId = pid;
    }

    process_handle() = default;
    ~process_handle()
    {
        ::CloseHandle(proc_info.hProcess);
        ::CloseHandle(proc_info.hThread);
    }
    process_handle(const process_handle & c) = delete;
    process_handle(process_handle && c) : proc_info(c.proc_info)
    {
        c.proc_info.hProcess = INVALID_HANDLE_VALUE;
        c.proc_info.hThread  = INVALID_HANDLE_VALUE;
    }
    process_handle &operator=(const process_handle & c) = delete;
    process_handle &operator=(process_handle && c)
    {
        ::CloseHandle(proc_info.hProcess);
        ::CloseHandle(proc_info.hThread);
        proc_info = c.proc_info;
        c.proc_info.hProcess = INVALID_HANDLE_VALUE;
        c.proc_info.hThread  = INVALID_HANDLE_VALUE;
        return *this;
    }

    pid_t id() const
    {
        return static_cast<int>(proc_info.dwProcessId);
    }

    typedef HANDLE process_handle_t;
    process_handle_t handle() const { return proc_info.hProcess; }

    bool valid() const
    {
        return (proc_info.hProcess != nullptr) &&
               (proc_info.hProcess != INVALID_HANDLE_VALUE);
    }


    inline bool is_running(int & exit_code) const
    {
        DWORD code;
        //single value, not needed in the winapi.
        if (!GetExitCodeProcess(proc_info.hProcess, &code))
            throw_last_error("GetExitCodeProcess() failed ", proc_info.dwProcessId);

        return code == still_active;
    }


    void terminate_if_running()
    {
        DWORD code;
        if (!GetExitCodeProcess(proc_info.hProcess, &code))
            return;
        TerminateProcess(proc_info.hProcess, EXIT_FAILURE);
    }

    void terminate() noexcept
    {
        if (!TerminateProcess(proc_info.hProcess, EXIT_FAILURE))
            throw_last_error("TerminateProcess() failed ", proc_info.dwProcessId);

        CloseHandle(proc_info.hProcess);
        proc_info.hProcess = INVALID_HANDLE_VALUE;

    }

    void wait(int & exit_code)
    {
        DWORD _exit_code = 1;

        if (WaitForSingleObject(proc_info.hProcess, INFINITE) == WAIT_FAILED)
            throw_last_error("WaitForSingleObject() failed ", proc_info.dwProcessId);


        if (!GetExitCodeProcess(proc_info.hProcess, &_exit_code))
            throw_last_error("GetExitCodeProcess() failed ", proc_info.dwProcessId);

        CloseHandle(proc_info.hProcess);
        proc_info.hProcess = INVALID_HANDLE_VALUE;
        exit_code = static_cast<int>(_exit_code);
    }


    std::optional<asio::windows::object_handle> ohandle;

    template<class Executor, class CompletionToken>
    auto async_wait(Executor& ctx, CompletionToken&& token, std::atomic<int> & exit_code)
    {
        ohandle.emplace(ctx, proc_info.hProcess);
        asio::async_completion<CompletionToken, void(int, std::error_code)> comp{std::forward<CompletionToken>(token)};

        ohandle->async_wait([this, &exit_code, h = std::move(comp.completion_handler)](std::error_code ec){
            if (ec)
                h(ec, 0);
            else
            {
                DWORD code;
                if (!GetExitCodeProcess(proc_info.hProcess, &code))
                    h(get_last_error(), 0);
                else
                {
                    exit_code.store(code);
                    h({}, static_cast<int>(code));
                }
            }
        });
    }

    void cancel_async_wait() { if (ohandle) ohandle->cancel(); }
};

}

#endif
