#ifndef PROCESS_PROCESS_GROUP_HPP
#define PROCESS_PROCESS_GROUP_HPP

#include <detail/process.hpp>

#if defined(__unix__)
#include <detail/process/posix/process_group.hpp>
#endif
#if defined(_WIN32) || defined(WIN32)
#include <detail/process/windows/process_group.hpp>
#endif

namespace PROCESS_NAMESPACE
{

class process_group
{
    detail::process::api::process_group_handle _group;
public:

    typedef typename detail::process::api::process_group_handle::native_handle_type native_handle_type;
    native_handle_type native_handle() const { return _group.native_handle(); }


    process_group() = default;
    explicit process_group(native_handle_type handle) : _group(handle) {}
    process_group(const process_group &) = delete;
    process_group(process_group && ) = default;

    process_group &operator=(const process_group &) = delete;
    process_group &operator=(process_group && ) = default;


    template<detail::process_initializer<default_process_launcher>  ... Inits>
    pid_type emplace(const std::filesystem::path& exe, std::initializer_list<std::wstring_view> args, Inits&&... inits)
    {
        return _group.emplace(exe, args, std::forward<Inits>(inits)...);
    }

    // Construct a child from a property list and launch it with a custom process launcher
    template<process_launcher Launcher, detail::process_initializer<Launcher> ... Inits>
    pid_type emplace(const std::filesystem::path& exe,
                     std::initializer_list<std::wstring_view> args,
                     Inits&&... inits,
                     Launcher&& launcher)
    {
        return _group.emplace(exe, args, std::forward<Inits>(inits)..., launcher);
    }

    // Construct a child from a property list and launch it.
    template<detail::process_initializer<default_process_launcher>  ... Inits>
    pid_type emplace(const std::filesystem::path& exe, std::initializer_list<std::string_view> args, Inits&&... inits)
    {
        return _group.emplace(exe, args, std::forward<Inits>(inits)...);
    }

    // Construct a child from a property list and launch it with a custom process launcher
    template<process_launcher Launcher, detail::process_initializer<Launcher> ... Inits>
    pid_type emplace(const std::filesystem::path& exe,
                     std::initializer_list<std::string_view> args,
                     Inits&&... inits,
                     Launcher&& launcher)
    {
        return _group.emplace(exe, args, std::forward<Inits>(inits)..., launcher);
    }

    // Construct a child from a property list and launch it.
    template<typename Args, detail::process_initializer<default_process_launcher>  ... Inits>
    pid_type emplace(const std::filesystem::path& exe, Args&& args, Inits&&... inits)
    {
        return _group.emplace(exe, std::forward<Args>(args), std::forward<Inits>(inits)...);
    }

    // Construct a child from a property list and launch it with a custom process launcher
    template<process_launcher Launcher, typename Args,
            detail::process_initializer<Launcher> ... Inits>
    pid_type emplace(const std::filesystem::path& exe,
                     Args&& args,
                     Inits&&... inits,
                     Launcher&& launcher)
    {
        return _group.emplace(exe, args, std::forward<Inits>(inits)..., launcher);
    }

    pid_type attach(process && proc) {return _group.attach(std::move(proc));};
    bool contains(pid_type proc)     {return _group.contains(proc);};

    void terminate() { _group.terminate(); }

    void wait()         { _group.wait(); }
    pid_type wait_one() { return _group.wait_one(); }
};

}

#endif //PROCESS_PROCESS_GROUP_HPP