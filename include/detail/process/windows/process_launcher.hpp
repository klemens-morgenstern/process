#ifndef DETAIL_PROCESS_WINDOWS_PROCESS_LAUNCHER_HPP
#define DETAIL_PROCESS_WINDOWS_PROCESS_LAUNCHER_HPP

#include <detail/process.hpp>

namespace PROCESS_NAMESPACE::detail::process::windows {


inline void append_cmd_line(std::wstring &ws, std::wstring_view arg)
{
    ws += arg;
    ws += L'\0';
}

inline void append_cmd_line(std::wstring &ws, std::string_view arg)
{
    wchar_t dummy;
    const auto req_size =
            MultiByteToWideChar(
                    CP_ACP, 0,
                    arg.data(),
                    arg.size(),
                    &dummy, 0);

    const auto current_size = ws.size();
    ws.resize(current_size + req_size + 1);

    MultiByteToWideChar(
            CP_ACP, 0,
            arg.data(),
            arg.size(),
            &ws[current_size], req_size);

    ws.back() = L'\0';
}


inline void append_cmd_line(std::wstring &ws, std::u8string_view arg)
{
    wchar_t dummy;
    const auto req_size =
            MultiByteToWideChar(
                    CP_UTF8, 0,
                    reinterpret_cast<const char*>(arg.data()),
                    arg.size(),
                    &dummy, 0);

    const auto current_size = ws.size();
    ws.resize(current_size + req_size + 1);

    MultiByteToWideChar(
            CP_ACP, 0,
            reinterpret_cast<const char*>(arg.data()),
            arg.size(),
            &ws[current_size], req_size);

    ws.back() = L'\0';
}

template<typename Args>
inline std::wstring build_cmd_line(Args && args) {
    std::wstring cmd_line;
    for (const auto & arg : args_in)
        append_cmd_line(c, arg);
    arg += L'\0';
}

template<typename Init, typename Launcher = default_process_launcher>
concept on_setup_init = requires(Init initializer, Launcher launcher) { {initializer.on_setup(launcher)}; };

template<typename Init, typename Launcher = default_process_launcher>
concept on_success_init = requires(Init initializer, Launcher launcher) { {initializer.on_success(launcher)}; };

template<typename Init, typename Launcher = default_process_launcher>
concept on_error_init = requires(Init initializer, Launcher launcher) { {initializer.on_error(launcher, std::error_code())}; };



class default_process_launcher
{
protected:
    std::error_code _ec;
    const char * _error_msg = nullptr;

    template<typename Initializer>
    void on_setup(Initializer &&initializer) {}

    template<typename Initializer> requires on_setup_init<Initializer>
    void on_setup(Initializer &&initializer) { init.on_setup(*this); }

    template<typename Initializer>
    void on_error(Initializer &&init) {}

    template<typename Initializer> requires on_error_init<Initializer>
    void on_error(Initializer &&init) { init.on_error(*this, _ec); }

    template<typename Initializer>
    void on_success(Initializer &&initializer) {}

    template<typename Initializer> requires on_success_init<Initializer>
    void on_success(Initializer &&init) { init.on_success(*this); }

public:
    void set_error(const std::error_code & ec, const char* msg = "Unknown Error.")
    {
        _ec = ec;
        _error_msg = msg;
    }

    template<typename Args, typename ... Inits>
    auto launch(std::filesystem::path exe, Args && args_in, Inits && ... inits) -> PROCESS_NAMESPACE::process
    {
        cmd_line = build_cmd_line(args_in);
        std::invoke([this](auto & init){on_setup(init);}, inits...);

        if (_ec)
        {
            std::invoke([this](auto & init){on_error(init);}, inits...);
            throw process_error(_ec, _error_msg, exe);
        }

        process proc{};
        int launched_process = ::boost::winapi::create_process(
                exe,                                        //       LPCSTR_ lpApplicationName,
                cmd_line.data(),                            //       LPSTR_ lpCommandLine,
                proc_attrs,                                 //       LPSECURITY_ATTRIBUTES_ lpProcessAttributes,
                thread_attrs,                               //       LPSECURITY_ATTRIBUTES_ lpThreadAttributes,
                inherit_handles,                            //       INT_ bInheritHandles,
                this->creation_flags,                       //       DWORD_ dwCreationFlags,
                reinterpret_cast<void*>(const_cast<Char*>(env)),  // LPVOID_ lpEnvironment,
                work_dir,                                   //       LPCSTR_ lpCurrentDirectory,
                &this->startup_info,                        //       LPSTARTUPINFOA_ lpStartupInfo,
                &proc._process_handle.proc_info);           //       LPPROCESS_INFORMATION_ lpProcessInformation)

        if (launched_process != 0)
        {
            _ec.clear();
            std::invoke([this](auto & init){on_success(init);}, inits...);
        }
        else
        {
            _ec = get_last_error();
            _msg = "CreateProcess failed"
            std::invoke([this](auto & init){on_error(init);}, inits...);
            throw process_error(_ec, _error_msg, exe);
        }

        if (_ec)
        {
            std::invoke([this](auto & init){this.on_error(init);}, inits...);
            throw process_error(_ec, _error_msg, exe);
        }
        return proc;
    }

    LPSECURITY_ATTRIBUTES proc_attrs   = nullptr;
    LPSECURITY_ATTRIBUTES thread_attrs = nullptr;
    BOOL inherit_handles = false;
    const wchar_t * work_dir = nullptr;
    std::wstring cmd_line = nullptr;
    const wchar_t * exe      = nullptr;
    const wchar_t * env      = nullptr;
    PROCESS_INFORMATION proc_info{nullptr, nullptr, 0,0};
};

}

#endif