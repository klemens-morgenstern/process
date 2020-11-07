#ifndef PROCESS_POSIX_PROCESS_GROUP_HPP
#define PROCESS_POSIX_PROCESS_GROUP_HPP

#include <utility>
#include <detail/process/exception.hpp>
#include <detail/process/config.hpp>
#include <process.hpp>


namespace PROCESS_NAMESPACE::detail::process::posix
{

struct process_group_handle
{
    pid_t grp = -1;

    typedef pid_t native_handle_type;
    native_handle_type native_handle() const { return grp; }

    explicit process_group_handle(native_handle_type h) : grp(h)
    {
    }

     process_group_handle() = default;
    ~process_group_handle() = default;

    process_group_handle(const process_group_handle & c) = delete;
    process_group_handle(process_group_handle && c) : grp(c.grp)
    {
        c.grp = -1;
    }
    process_group_handle &operator=(const process_group_handle & c) = delete;
    process_group_handle &operator=(process_group_handle && c)
    {
        grp = c.grp;
        c.grp = -1;
        return *this;
    }

    pid_type attach(PROCESS_NAMESPACE::process && proc)
    {
        if (::setpgid(proc.id(), grp))
            throw_last_error("setpgid failed");
        proc.detach();
        return proc.id();
    }

    template<typename Args,
            detail::process_initializer<default_process_launcher> ... Inits>
    inline pid_type emplace(const std::filesystem::path& exe,
                     Args&& args,
                     Inits&&... inits);
    template<process_launcher Launcher, typename Args,
            detail::process_initializer<Launcher> ... Inits>
    inline pid_type emplace(const std::filesystem::path& exe,
                     Args&& args,
                     Inits&&... inits,
                     Launcher&& launcher);


    bool contains(pid_type proc)
    {
        return ::getpgid(proc) == grp;
    }

    void terminate()
    {
        if (::killpg(grp, SIGKILL) == -1)
            throw_last_error("killpg() failed");
        grp = -1;
    }

    void wait()
    {
        pid_t ret;
        siginfo_t  status;

        do
        {
            ret = ::waitpid(-grp, &status.si_status, 0);
            if (ret == -1)
                throw_last_error("waitpid failed");

            //ECHILD --> no child processes left.
            ret = ::waitid(P_PGID, grp, &status, WEXITED | WNOHANG);
        }
        while ((ret != -1) || (errno != ECHILD));

        if (errno != ECHILD)
            throw_last_error("waitpid failed");
    }

    std::pair<pid_type, int> wait_one()
    {
        pid_t ret;
        int  status;

        ret = ::waitpid(-grp, &status, 0);
        if (ret == -1)
            throw_last_error("waitpid failed");
        return {ret, WEXITSTATUS(status)};
    }
};

struct group_ref
{
    process_group_handle & grp;

    explicit inline group_ref(process_group_handle &g) : grp(g) {}

    template <class Executor>
    void on_exec_setup(Executor&) const
    {
        if (grp.grp == -1)
            ::setpgid(0, 0);
        else
            ::setpgid(0, grp.grp);
    }

    template <class Executor>
    void on_success(Executor& exec) const
    {
        if (grp.grp == -1)
            grp.grp = exec.pid;

    }
};


template<typename Args,
        detail::process_initializer<default_process_launcher> ... Inits>
pid_type process_group_handle::emplace(const std::filesystem::path& exe,
                 Args&& args,
                 Inits&&... inits)
{
    auto proc = default_process_launcher{}.launch(exe, args, std::forward<Inits>(inits)..., group_ref{*this});
    proc.detach();
    return proc.id();
}

template<process_launcher Launcher, typename Args,
        detail::process_initializer<Launcher> ... Inits>
pid_type process_group_handle::emplace(const std::filesystem::path& exe,
                 Args&& args,
                 Inits&&... inits,
                 Launcher&& launcher)
{
    auto proc = launcher.launch(exe, args, std::forward<Inits>(inits)..., group_ref{*this});
    proc.detach();
    return proc.id();
}


}

#endif //PROCESS_WINDOWS_PROCESS_GROUP_HPP
