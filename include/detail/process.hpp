#ifndef PROCESS_HPP
#define PROCESS_HPP

#include <chrono>
#include <filesystem>
#include <ranges>
#include <string>
#include <system_error>
#include <vector>
#include <string_view>
#include <atomic>

#include <detail/process/config.hpp>
#include <detail/process/exception.hpp>

#if defined(__unix__)
#include <detail/process/posix/process_handle.hpp>
#endif
#if defined(_WIN32) || defined(WIN32)
#include <detail/process/windows/process_handle.hpp>
#endif

namespace PROCESS_NAMESPACE
{

class process;

namespace detail {

template<typename Launcher, typename Init>
concept process_initializer =
   (  requires(Init initializer, Launcher launcher) { {initializer.on_setup(launcher)}; }
   || requires(Init initializer, Launcher launcher) { {initializer.on_success(launcher)}; }
   || requires(Init initializer, Launcher launcher) { {initializer.on_error(launcher, std::error_code())}; }
   );

}

template<typename Launcher, typename Args = std::vector<std::string>, typename ...Initializers>
concept process_launcher =
    (
       std::convertible_to<std::iter_value_t<std::ranges::iterator_t<Args>>, std::string_view> ||
       std::convertible_to<std::iter_value_t<std::ranges::iterator_t<Args>>, std::wstring_view> ||
       std::convertible_to<std::iter_value_t<std::ranges::iterator_t<Args>>, std::u8string_view>
    )  &&
    requires(Launcher launcher, Args a, ::std::filesystem::path p, Initializers ... initializers) {
            { launcher.set_error(std::error_code(), "message") };
            { launcher.launch(p, a, initializers...) } -> std::same_as<process>;
        }
        &&  (detail::process_initializer<Launcher, Initializers> &&  ...);

class default_process_launcher;

namespace detail {
template<typename T> using same = T;
}

class process
{
    bool _attached{true};
    bool _terminated{false};
    mutable std::atomic<int> _exit_status{detail::process::api::still_active};
    //Let's make it easy for now
public:
    detail::process::api::process_handle _process_handle;
public:
    // Provides access to underlying operating system facilities
    using native_handle_type = detail::process::native_handle_type;

    // Construct a child from a property list and launch it.
    template<detail::process_initializer<default_process_launcher>  ... Inits>
    explicit process(const std::filesystem::path& exe, std::initializer_list<std::wstring_view> args, Inits&&... inits);

    // Construct a child from a property list and launch it with a custom process launcher
    template<process_launcher Launcher, detail::process_initializer<Launcher> ... Inits>
    explicit process(const std::filesystem::path& exe,
                     std::initializer_list<std::wstring_view> args,
                     Inits&&... inits,
                     Launcher&& launcher) : process(launcher.launch(exe, args, std::forward<Inits>(inits)...)) {}

    // Construct a child from a property list and launch it.
    template<detail::process_initializer<default_process_launcher>  ... Inits>
    explicit process(const std::filesystem::path& exe, std::initializer_list<std::string_view> args, Inits&&... inits);

    // Construct a child from a property list and launch it with a custom process launcher
    template<process_launcher Launcher, detail::process_initializer<Launcher> ... Inits>
    explicit process(const std::filesystem::path& exe,
                     std::initializer_list<std::string_view> args,
                     Inits&&... inits,
                     Launcher&& launcher) : process(launcher.launch(exe, args, std::forward<Inits>(inits)...)) {}

    // Construct a child from a property list and launch it.
    template<typename Args, detail::process_initializer<default_process_launcher>  ... Inits>
    explicit process(const std::filesystem::path& exe, Args&& args, Inits&&... inits);

    // Construct a child from a property list and launch it with a custom process launcher
    template<process_launcher Launcher, typename Args,
             detail::process_initializer<Launcher> ... Inits>
    explicit process(const std::filesystem::path& exe,
                     Args&& args,
                     Inits&&... inits,
                     Launcher&& launcher) : process(launcher.launch(exe, args, std::forward<Inits>(inits)...)) {}

    // Attach to an existing process
    explicit process(const pid_type& pid) : _process_handle{pid} {}
    // An empty process is similar to a default constructed thread. It holds an empty
    // handle and is a place holder for a process that is to be launched later.
    process() = default;

    process(const process&) = delete;
    process& operator=(const process&) = delete;

    process(process&& lhs) : _attached(lhs._attached), _terminated(lhs._terminated), _exit_status{lhs._exit_status.load()}, _process_handle(std::move(lhs._process_handle))
    {
        lhs._attached = false;
    }
    process& operator=(process&& lhs)
    {
        _attached = lhs._attached;
        _terminated = lhs._terminated;
        _exit_status.store(lhs._exit_status.load());
        _process_handle = std::move(lhs._process_handle);
        lhs._attached = false;
        return *this;
    }
    // tbd behavior
    ~process() {
        if (_attached && !_terminated)
            _process_handle.terminate_if_running();
    }
    // Accessors
    pid_type id() const                      {return _process_handle.id();}
    native_handle_type native_handle() const {return _process_handle.handle();}
    // Return code of the process, only valid if !running()
    int exit_code() const { return detail::process::api::eval_exit_status(_exit_status.load());}
    // Return the system native exit code. That is on Linux it contains the
    // reason of the exit, such as can be detected by WIFSIGNALED
    int native_exit_code() const { return _exit_status.load();}
    // Check if the process is running. If the process has exited already, it might store
    // the exit_code internally.
    bool running() const
    {
        int status = _exit_status.load();
        const auto res = _process_handle.is_running(status);

        _exit_status.store(status);
        return res;
    }
    // Check if this handle holds a child process.
    // NOTE: That does not mean, that the process is still running. It only means, that
    // the handle does or did exist.
    bool valid() const { return _process_handle.valid(); }
    // Process management functions
    // Detach a spawned process -- let it run after this handle destructs
    void detach() {_attached = false; }
    // Terminate the child process (child process will unconditionally and immediately exit)
    // Implemented with SIGKILL on POSIX and TerminateProcess on Windows
    void terminate() { _process_handle.terminate(); _terminated = true; }
    // Block until the process to exits
    void wait()
    {
        int status = 0;
        _process_handle.wait(status);
        _exit_status.store(status);
    }

    // The following is dependent on the networking TS. CompletionToken has the signature
    // (int, error_code), i.e. wait for the process to exit and get the exit_code if exited.
    template<class Executor, class CompletionToken>
    auto async_wait(Executor& ctx, CompletionToken&& token)
    {
        return _process_handle.async_wait(ctx, std::forward<CompletionToken>(token), _exit_status);
    }
    // Cancel
    void cancel_async_wait() {
        _process_handle.cancel_async_wait();
    }
};

}

#include <detail/process_launcher.hpp>

namespace PROCESS_NAMESPACE {
template<typename Args, detail::process_initializer<default_process_launcher>  ... Inits>
process::process(const std::filesystem::path& exe, Args&& args, Inits&&... inits)
    : process(default_process_launcher{}.launch(exe, args, std::forward<Inits>(inits)...)) {}

// Construct a child from a property list and launch it.
template<detail::process_initializer<default_process_launcher>  ... Inits>
process::process(const std::filesystem::path& exe, std::initializer_list<std::string_view> args, Inits&&... inits)
    : process(default_process_launcher{}.launch(exe, args, std::forward<Inits>(inits)...)) {}

template<detail::process_initializer<default_process_launcher>  ... Inits>
process::process(const std::filesystem::path& exe, std::initializer_list<std::wstring_view> args, Inits&&... inits)
        : process(exe, args, std::forward<Inits>(inits)...) {}



}



#endif //PROCESS_HPP
