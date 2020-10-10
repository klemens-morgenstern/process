#ifndef PROCESS_DETAIL_CONFIG_HPP
#define PROCESS_DETAIL_CONFIG_HPP

#include <system_error>


#if defined(__unix__)
#include <errno.h>
#if defined(_WIN32) || defined(WIN32)
#include <features.h>
#endif
#elif defined(_WIN32) || defined(WIN32)
#include <asio/windows/object_handle.hpp>
#include <windows.h>
#else
#error "System API not supported by process"
#endif

namespace PROCESS_NAMESPACE::detail::process
{

#if !defined(DEFAULT_PIPE_SIZE)
#define DEFAULT_PIPE_SIZE 1024
#endif

#if defined(__unix__)
namespace posix {namespace extensions {}}
namespace api = posix;

inline std::error_code get_last_error() noexcept
{
    return std::error_code(errno, std::system_category());
}

typedef ::pid_t pid_type;

typedef pid_t native_handle_type;
#elif defined(_WIN32) || defined(WIN32)
namespace windows {namespace extensions {}}
namespace api = windows;

inline std::error_code get_last_error() noexcept
{
    return std::error_code(GetLastError(), std::system_category());
}

typedef int pid_type;
typedef HANDLE native_handle_type;
#endif

template<typename Char> constexpr Char null_char;
template<> constexpr char     null_char<char>    = '\0';
template<> constexpr wchar_t  null_char<wchar_t> = L'\0';

template<typename Char> constexpr Char equal_sign;
template<> constexpr char     equal_sign<char>    =  '=';
template<> constexpr wchar_t  equal_sign<wchar_t> = L'=';

template<typename Char> constexpr Char quote_sign;
template<> constexpr char     quote_sign<char>    =  '"';
template<> constexpr wchar_t  quote_sign<wchar_t> = L'"';

template<typename Char> constexpr Char space_sign;
template<> constexpr char     space_sign<char>    =  ' ';
template<> constexpr wchar_t  space_sign<wchar_t> = L' ';

}

namespace PROCESS_NAMESPACE { typedef detail::process::pid_type pid_type; }

#endif
