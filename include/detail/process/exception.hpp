#ifndef PROCESS_EXCEPTION_HPP_
#define PROCESS_EXCEPTION_HPP_

#include <system_error>
#include <detail/process/config.hpp>

namespace PROCESS_NAMESPACE
{

class process_error : public std::system_error {
    std::filesystem::path _path{};
    pid_type _pid{0};
public:
    // filesystem_error can take up to two paths in case of an error
    // In the same vein, process_error can take a path or a pid
    using std::system_error::system_error;

    process_error(std::error_code ec, const std::string& what_arg, const std::filesystem::path& path) : std::system_error(ec, "[" + path.string() + "]: " + what_arg ), _path{path} {}
    process_error(std::error_code ec, const std::string& what_arg, pid_type pid_arg) : std::system_error(ec, "PID [" + std::to_string(pid_arg) + "]: " + what_arg ), _pid{pid_arg} {}
    const std::filesystem::path& path() const noexcept {return _path;}
    pid_type pid() const noexcept {return _pid;}
};

namespace detail::process
{
inline void throw_last_error(const std::string & what_arg)
{
    throw process_error(get_last_error(), what_arg);
}

inline void throw_last_error(const std::string & what_arg, const std::filesystem::path& path)
{
    throw process_error(get_last_error(), what_arg, path);
}

inline void throw_last_error(const std::string & what_arg, pid_type pid_arg)
{
    throw process_error(get_last_error(), what_arg, pid_arg);
}

}


}

#endif /* PROCESS_EXCEPTION_HPP_ */
