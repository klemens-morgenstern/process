#ifndef PROCESS_PROCESS_IO_HPP
#define PROCESS_PROCESS_IO_HPP

#include <detail/process/config.hpp>

#if defined(_WIN32) || defined(WIN32)
#include <io.h>
#endif

namespace PROCESS_NAMESPACE
{

template<typename T>
struct process_io_traits;


namespace detail {
#if defined(__unix__)

using native_stream_handle = int;

struct default_stderr { static int get() {return STDERR_FILENO;} };
struct default_stdout { static int get() {return STDOUT_FILENO;} };
struct default_stdin  { static int get() {return STDIN_FILENO;} };

#else

using native_stream_handle = HANDLE;

struct default_stderr { static HANDLE get() {return GetStdHandle(STD_ERROR_HANDLE);} };
struct default_stdout { static HANDLE get() {return GetStdHandle(STD_OUTPUT_HANDLE);} };
struct default_stdin  { static HANDLE get() {return GetStdHandle(STD_INPUT_HANDLE);} };

#endif
}

template<>
struct process_io_traits<detail::default_stdin> {
    static auto get_readable_handle(detail::default_stdin v) {return v.get();}
};

template<>
struct process_io_traits<detail::default_stdout> {
    static auto get_writable_handle(detail::default_stdout v) {return v.get();}
};

template<>
struct process_io_traits<detail::default_stderr> {
    static auto get_writable_handle(detail::default_stderr v) {return v.get();}
};

template<>
struct process_io_traits<FILE*> {

#if defined(__unix__)
    static auto get_readable_handle (FILE* p) {return fileno(p);}
    static auto get_writable_handle(FILE* p) {return fileno(p);}
#else
    static auto get_readable_handle (FILE* p) {return reinterpret_cast<void*>(_get_osfhandle(_fileno(p)));}
    static auto get_writable_handle(FILE* p) {return reinterpret_cast<void*>(_get_osfhandle(_fileno(p)));}
#endif
};

namespace detail
{
#if defined(__unix__)
struct file_descriptor
{
    enum mode_t
    {
        read  = 1,
        write = 2,
        read_write = 3
    };


    explicit file_descriptor(const std::filesystem::path& p, mode_t mode = read_write)
            : _handle(open_file(p.c_str(), mode))
    {
    }

    file_descriptor(const file_descriptor & ) = delete;
    file_descriptor(file_descriptor && lhs) : _handle(lhs._handle)
    {
        lhs._handle  = -1;
    }

    file_descriptor& operator=(const file_descriptor & ) = delete;

    file_descriptor& operator=(file_descriptor && lhs)
    {
        _handle = lhs._handle;
        lhs._handle  = -1;
        return *this;
    }

    ~file_descriptor()
    {
        if (_handle != -1)
            ::close(_handle);
    }
    operator int () const {return _handle;}
    int handle() const { return _handle;}

private:
    static int open_file(const char* name, mode_t mode )
    {
        switch(mode)
        {
            case read:  return ::open(name, O_RDONLY);
            case write: return ::open(name, O_WRONLY | O_CREAT, 0660);
            case read_write: return ::open(name, O_RDWR | O_CREAT, 0660);
            default: return -1;
        }
    }
    int _handle = -1;
};
#else

struct file_descriptor
{
    enum mode_t
    {
        read  = 1,
        write = 2,
        read_write = 3
    };
    static ::DWORD desired_access(mode_t mode)
    {
        switch(mode)
        {
            case read:       return GENERIC_READ;
            case write:      return GENERIC_WRITE;
            case read_write: return GENERIC_READ | GENERIC_WRITE;
            default:         return 0u;
        }
    }

    file_descriptor() = default;
    file_descriptor(const std::filesystem::path& p, mode_t mode = read_write)
            : _handle(
            CreateFileW(
                    p.c_str(),
                    desired_access(mode),
                    FILE_SHARE_READ |
                    FILE_SHARE_WRITE,
                    nullptr,
                    OPEN_ALWAYS,

                    FILE_ATTRIBUTE_NORMAL,
                    nullptr
            ))
    {
    }
    file_descriptor(const file_descriptor & ) = delete;
    file_descriptor(file_descriptor &&other)
            : _handle(other._handle)
    {
        other._handle = INVALID_HANDLE_VALUE;
    }

    file_descriptor& operator=(const file_descriptor & ) = delete;
    file_descriptor& operator=(file_descriptor &&other)
    {
        if (_handle != INVALID_HANDLE_VALUE)
            CloseHandle(_handle);
        _handle = other._handle;
        return *this;
    }

    ~file_descriptor()
    {
        if (_handle != INVALID_HANDLE_VALUE)
            CloseHandle(_handle);
    }

    HANDLE handle() const { return _handle;}
    operator HANDLE () const { return _handle;}

private:
    HANDLE _handle = INVALID_HANDLE_VALUE;
};

#endif

}



template<>
struct process_io_traits<std::filesystem::path> {

    static auto get_readable_handle(const std::filesystem::path & p) {return detail::file_descriptor{p, detail::file_descriptor::read};}
    static auto get_writable_handle(const std::filesystem::path & p) {return detail::file_descriptor{p, detail::file_descriptor::write};}
};

template<typename In = detail::default_stdin, typename Out = detail::default_stdout, typename Err = detail::default_stderr>
requires(
       requires(In   in) { { process_io_traits<std::remove_reference_t<In >>::get_readable_handle(in) } -> std::convertible_to<detail::native_stream_handle>;}
    && requires(Out out) { { process_io_traits<std::remove_reference_t<Out>>::get_writable_handle(out)} -> std::convertible_to<detail::native_stream_handle>;}
    && requires(Err err) { { process_io_traits<std::remove_reference_t<Err>>::get_writable_handle(err)} -> std::convertible_to<detail::native_stream_handle>;}
)
struct process_io
{
    In   in = {};
    Out out = {};
    Err err = {};

    using native_stream_handle = detail::native_stream_handle;

    mutable std::optional<std::tuple<
            decltype(process_io_traits<std::remove_reference_t<In>> ::get_readable_handle(in)),
            decltype(process_io_traits<std::remove_reference_t<Out>>::get_writable_handle(out)),
            decltype(process_io_traits<std::remove_reference_t<Err>>::get_writable_handle(err))>> _buffer;

    auto & _get_handles() const
    {
        if (!_buffer)
            _buffer.emplace(
                    process_io_traits<std::remove_reference_t<In>> ::get_readable_handle(in),
                    process_io_traits<std::remove_reference_t<Out>>::get_writable_handle(out),
                    process_io_traits<std::remove_reference_t<Err>>::get_writable_handle(err));

        return *_buffer;
    }
#if defined(__unix__)
    template <typename Executor>
    void on_exec_setup(Executor &e) const
    {
        auto & [h_in_, h_out_, h_err_] = _get_handles();
        if (auto h_in = static_cast<int>(h_in_);
            (h_in != STDIN_FILENO) && (::dup2(h_in, STDIN_FILENO) == -1))
            e.set_error(detail::process::get_last_error(), "dup2(stdin) failed");

        if (auto h_out = static_cast<int>(h_out_);
            (h_out != STDOUT_FILENO) && (::dup2(h_out, STDOUT_FILENO) == -1))
            e.set_error(detail::process::get_last_error(), "dup2(stdout) failed");

        if (auto h_err = static_cast<int>(h_err_);
            (h_err != STDERR_FILENO) && (::dup2(h_err, STDERR_FILENO) == -1))
            e.set_error(detail::process::get_last_error(), "dup2(stderr) failed");

    }
    template <typename Launcher>
    void on_setup(const Launcher &e) {}
#else
    template <typename Executor>
    void on_setup(Executor &e) const
    {
        auto & [h_in_, h_out_, h_err_] = _get_handles();
        auto h_in = static_cast<HANDLE>(h_in_), h_out = static_cast<HANDLE>(h_out_), h_err = static_cast<HANDLE>(h_err_);
        SetHandleInformation(h_in,  HANDLE_FLAG_INHERIT, HANDLE_FLAG_INHERIT);
        SetHandleInformation(h_out, HANDLE_FLAG_INHERIT, HANDLE_FLAG_INHERIT);
        SetHandleInformation(h_err, HANDLE_FLAG_INHERIT, HANDLE_FLAG_INHERIT);

        e.startup_info.dwFlags |= STARTF_USESTDHANDLES;

        e.startup_info.hStdInput  = h_in;
        e.startup_info.hStdOutput = h_out;
        e.startup_info.hStdError  = h_err;
    }

#endif
};

}

#endif //PROCESS_PROCESS_IO_HPP
