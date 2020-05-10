#ifndef PROCESS_LAUNCHER_HPP
#define PROCESS_LAUNCHER_HPP

#if defined(__unix__)
#include <detail/process/posix/process_launcher.hpp>
#endif
#if defined(_WIN32) || defined(WIN32)
#include <detail/process/windows/process_launcher.hpp>
#endif

namespace PROCESS_NAMESPACE
{
    class default_process_launcher : detail::process::api::default_process_launcher
    {
    public:
        using detail::process::api::default_process_launcher::set_error;
        using detail::process::api::default_process_launcher::launch;
    };

    static_assert(process_launcher<default_process_launcher>);
}

#endif //PROCESS_HPP
