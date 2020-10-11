#ifndef DETAIL_PROCESS_HANDLE_HPP
#define DETAIL_PROCESS_HANDLE_HPP

#include <wait.h>
#include <detail/process/exception.hpp>
#include <unistd.h>
#include <asio/signal_set.hpp>


namespace PROCESS_NAMESPACE::detail::process::posix {

constexpr int still_active = 0x017f;
static_assert(WIFSTOPPED(still_active), "Expected still_active to indicate WIFSTOPPED");
static_assert(!WIFEXITED(still_active), "Expected still_active to not indicate WIFEXITED");
static_assert(!WIFSIGNALED(still_active), "Expected still_active to not indicate WIFSIGNALED");
static_assert(!WIFCONTINUED(still_active), "Expected still_active to not indicate WIFCONTINUED");


constexpr inline int eval_exit_status(int code)
{
    if (WIFEXITED(code))
    {
        return WEXITSTATUS(code);
    }
    else if (WIFSIGNALED(code))
    {
        return WTERMSIG(code);
    }
    else
    {
        return code;
    }
}

inline bool is_code_running(int code)
{
    return !WIFEXITED(code) && !WIFSIGNALED(code);
}

struct process_handle
{
    int pid {-1};
    explicit process_handle(int pid) : pid(pid)
    {}

    process_handle()  = default;
    ~process_handle() = default;

    process_handle(const process_handle & c) = delete;
    process_handle(process_handle && c) : pid(c.pid)
    {
        c.pid = -1;
    }
    process_handle &operator=(const process_handle & c) = delete;
    process_handle &operator=(process_handle && c)
    {
        pid = c.pid;
        c.pid = -1;
        return *this;
    }

    int id() const
    {
        return pid;
    }
    
    typedef int process_handle_t;
    process_handle_t handle() const { return pid; }

    bool valid() const
    {
        return pid != -1;
    }

    inline bool is_running(int & exit_code) const
    {
        if (!is_code_running(exit_code))
            return false;
        auto ret = ::waitpid(pid, &exit_code, WNOHANG);

        if (ret == -1)
            throw_last_error("waitpid() failed", pid);
        return is_code_running(exit_code);
    }


    void terminate_if_running()
    {
        int exit_code;
        if (!is_code_running(exit_code))
            return;
        auto ret = ::waitpid(pid, &exit_code, WNOHANG);

        if (is_code_running(exit_code))
            ::kill(pid, SIGKILL);
    }

    void terminate() noexcept
    {
        if (::kill(pid, SIGKILL) == -1)
            throw_last_error("terminate() failed", pid);

        ::waitpid(pid, nullptr, WNOHANG); //just to clean it up
    }

    void wait(int & exit_code)
    {
        pid_t ret;
        int status;

        do
            ret = ::waitpid(pid, &status, 0);
        while (((ret == -1) && (errno == EINTR)) ||
               (ret != -1 && !WIFEXITED(status) && !WIFSIGNALED(status)));

        if (ret == -1)
            throw_last_error("waitpid() failed", pid);
        exit_code = status;
    }

    std::optional<asio::signal_set> sset;

    template<class Executor, class CompletionToken>
    auto async_wait(Executor& ctx, CompletionToken && token, std::atomic<int> & exit_code)
    {
        sset.emplace(ctx, SIGCHLD);

        asio::async_completion<CompletionToken, void(int, std::error_code)> comp{token};
        _check_status(std::move(comp.completion_handler), {}, exit_code);

        return comp.result.get();
    }

    void cancel_async_wait() { if (sset) sset->cancel(); }

private:
    template<typename Handler>
    void _check_status(Handler && h, std::error_code ec, std::atomic<int> & exit_code)
    {
        if (ec)
            h(ec, 0);
        int status{exit_code.load()};
        int ret = waitpid(pid, &status, WNOHANG);

        if (ret < 0)
        {
            h(get_last_error(), 0);
            sset = std::nullopt;
        }
        else if (ret == pid && !is_code_running(status))
        {
            exit_code = status;
            h({}, eval_exit_status(status));
            sset = std::nullopt;
        }
        else
            sset->async_wait([this, &exit_code, h = std::move(h)](std::error_code ec, int) mutable { _check_status(std::move(h), ec, exit_code); });
    }


};

};

#endif //DETAIL_PROCESS_HANDLE_HPP
