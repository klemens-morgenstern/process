#ifndef PROCESS_WINDOWS_PROCESS_GROUP_HPP
#define PROCESS_WINDOWS_PROCESS_GROUP_HPP

#include <windows.h>
#include <utility>
#include <detail/process/exception.hpp>
#include <detail/process/config.hpp>
#include <process.hpp>

namespace PROCESS_NAMESPACE::detail::process::windows
{

inline bool break_away_enabled(HANDLE h)
{
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION info;

    if (!QueryInformationJobObject(
            h,
            JobObjectExtendedLimitInformation,
            static_cast<void*>(&info),
            sizeof(info),
            nullptr))
        throw_last_error("QueryInformationJobObject() failed");

    return (info.BasicLimitInformation.LimitFlags & JOB_OBJECT_LIMIT_BREAKAWAY_OK) != 0;
}

inline void enable_break_away(HANDLE h)
{
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION info;

    if (!QueryInformationJobObject(
            h,
            JobObjectExtendedLimitInformation,
            static_cast<void*>(&info),
            sizeof(info),
            nullptr))
        throw_last_error("QueryInformationJobObject() failed");

    if ((info.BasicLimitInformation.LimitFlags & JOB_OBJECT_LIMIT_BREAKAWAY_OK) != 0)
        return;

    info.BasicLimitInformation.LimitFlags |= JOB_OBJECT_LIMIT_BREAKAWAY_OK;

    if (!SetInformationJobObject(
            h,
            JobObjectExtendedLimitInformation,
            static_cast<void*>(&info),
            sizeof(info)))
        throw_last_error("SetInformationJobObject() failed");
}

inline void associate_completion_port(HANDLE job,
                                      HANDLE io_port)
{
    JOBOBJECT_ASSOCIATE_COMPLETION_PORT port;
    port.CompletionKey  = job;
    port.CompletionPort = io_port;

    if (!SetInformationJobObject(
            job,
            JobObjectAssociateCompletionPortInformation,
            static_cast<void*>(&port),
            sizeof(port)))
        throw_last_error("SetInformationJobObject() failed");
}

inline bool in_group()
{
    BOOL res;
    if (!IsProcessInJob(GetCurrentProcess(), nullptr, &res))
        throw_last_error("IsProcessInJob failed");

    return res!=0;
}


inline HANDLE get_process_handle(pid_type pid)
{
    auto h = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid);
    if (h == nullptr)
        throw_last_error("OpenProcess() failed");
    return h;
}

struct process_group_handle;

struct group_ref
{
    HANDLE handle;

    explicit inline group_ref(process_group_handle &g);

    template <class Executor>
    void on_setup(Executor& exec) const
    {
        //I can only enable this if the current process supports breakaways.
        if (in_group() && break_away_enabled(nullptr))
            exec.creation_flags  |= CREATE_BREAKAWAY_FROM_JOB;
    }


    template <class Executor>
    void on_success(Executor& exec) const
    {
        if (!AssignProcessToJobObject(handle, exec.proc_info.hProcess))
            exec.set_error(get_last_error(), "AssignProcessToJobObject() failed.");
    }

};


class process_group_handle
{
    HANDLE _job_object;
    HANDLE _io_port;
public:
    typedef typename HANDLE native_handle_type;
    native_handle_type native_handle() const { return _job_object; }

    explicit process_group_handle(native_handle_type h) : _job_object(h),
            _io_port(CreateIoCompletionPort(INVALID_HANDLE_VALUE, nullptr, 0, 1))
    {
        enable_break_away(_job_object);
        associate_completion_port(_job_object, _io_port);
    }

    process_group_handle() : process_group_handle(CreateJobObjectW(nullptr, nullptr))
    {
    }

    ~process_group_handle()
    {
        CloseHandle(_job_object);
        CloseHandle(_io_port);
    }

    process_group_handle(const process_group_handle & c) = delete;
    process_group_handle(process_group_handle && c) : _job_object(c._job_object), _io_port(c._io_port)
    {
        c._job_object = INVALID_HANDLE_VALUE;
        c._io_port    = INVALID_HANDLE_VALUE;
    }
    process_group_handle &operator=(const process_group_handle & c) = delete;
    process_group_handle &operator=(process_group_handle && c)
    {
        CloseHandle(_io_port);
        _io_port = c._io_port;
        c._io_port = INVALID_HANDLE_VALUE;

        CloseHandle(_job_object);
        _job_object = c._job_object;
        c._job_object = INVALID_HANDLE_VALUE;
        return *this;
    }

    pid_type attach(PROCESS_NAMESPACE::process && proc)
    {

        if (!AssignProcessToJobObject(_job_object, proc.native_handle()))
            throw_last_error("AssignProcessToJobObject failed");
        proc.detach();
        return proc.id();
    }

    template<typename Args,
            detail::process_initializer<Launcher> ... Inits>
    pid_type emplace(const std::filesystem::path& exe,
                     Args&& args,
                     Inits&&... inits)
    {
        return attach(default_process_launcher{}.launch(exe, args, std::forward<Inits>(inits)..., group_ref{*this}));
    }

    template<process_launcher Launcher, typename Args,
            detail::process_initializer<default_process_launcher> ... Inits>
    pid_type emplace(const std::filesystem::path& exe,
                     Args&& args,
                     Inits&&... inits,
                     Launcher&& launcher)
    {
        return attach(launcher.launch(exe, args, std::forward<Inits>(inits)..., group_ref{*this}));
    }


    bool contains(pid_type proc)
    {
        BOOL is;
        if (!IsProcessInJob(get_process_handle(proc), _job_object,  &is))
            throw_last_error("IsProcessInJob Failed");

        return is!=0;
    }

    void terminate()
    {
        if (!TerminateJobObject(_job_object, EXIT_FAILURE))
            throw_last_error("TerminateJobObject() failed");
    }

    void wait()
    {
        DWORD completion_code;
        ULONG_PTR completion_key;
        LPOVERLAPPED overlapped;

        while (GetQueuedCompletionStatus(
                _io_port, &completion_code,
                &completion_key, &overlapped, INFINITE))
        {
            if (reinterpret_cast<HANDLE>(completion_key) == _job_object &&
                completion_code == JOB_OBJECT_MSG_ACTIVE_PROCESS_ZERO)
            {

                //double check, could be a different handle from a child
                JOBOBJECT_BASIC_ACCOUNTING_INFORMATION info;
                if (!QueryInformationJobObject(
                        _job_object,
                        JobObjectBasicAccountingInformation,
                        static_cast<void *>(&info),
                        sizeof(info), nullptr))
                    throw_last_error("QueryInformationJobObject failed");
                else if (info.ActiveProcesses == 0)
                    return; //correct, nothing left.
            }
        }
    }

    std::pair<pid_type, int>  wait_one()
    {
        DWORD completion_code;
        ULONG_PTR completion_key;
        LPOVERLAPPED overlapped;

        while (GetQueuedCompletionStatus(
                _io_port, &completion_code,
                &completion_key, &overlapped, INFINITE))
        {
            if (reinterpret_cast<HANDLE>(completion_key) == _job_object && completion_code == JOB_OBJECT_MSG_EXIT_PROCESS)
            {
                auto pid = static_cast<pid_type>(reinterpret_cast<ULONG_PTR>(overlapped));
                DWORD code;
                if (!GetExitCodeProcess(get_process_handle(pid), &code))
                    throw_last_error("GetExitCodeProcess() failed");
                return {pid, static_cast<int>(code)};
            }

        }
        throw_last_error("GetQueuedCompletionStatus failed");
        return {-1, -1};
    }
};

group_ref::group_ref(process_group_handle &g) : handle(g.native_handle())
{}


}

#endif //PROCESS_WINDOWS_PROCESS_GROUP_HPP
