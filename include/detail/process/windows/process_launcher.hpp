#ifndef DETAIL_PROCESS_WINDOWS_PROCESS_LAUNCHER_HPP
#define DETAIL_PROCESS_WINDOWS_PROCESS_LAUNCHER_HPP

#include <detail/process.hpp>
#include <functional>

namespace PROCESS_NAMESPACE::detail::process::windows {

inline std::wstring_view as_wstring(std::wstring_view in) {return in.data();}
inline std::wstring as_wstring(std::string_view in)
{
    return {in.begin(), in.end()};
}

template<typename Args>
inline std::wstring build_args(const std::filesystem::path & exe, Args && data)
{
    std::wstring st = exe;

    //put in quotes if it has spaces or double quotes
    if(!exe.empty())
    {
        auto it = st.find_first_of(L" \"");

        if(it != st.npos)//contains spaces or double quotes.
        {
            // double existing quotes
            for (auto p = st.find(L'"'); p != std::wstring::npos; p = st.find(L'"', p + 2))
                st.replace(p, 1, L"\"\"");

            // surround with quotes
            st.insert(st.begin(), L'"');
            st += L'"';
        }
    }

    st += L' ';

    for(auto & arg_ : data)
    {
        if(!arg_.empty())
        {
            auto warg = as_wstring(arg_);
            auto it = warg.find_first_of(L" \"");//contains space or double quotes?
            if(it != warg.npos) //yes
            {
                std::wstring arg{warg.data()};
                // double existing quotes
                for (auto p = arg.find(L'"'); p != decltype(arg)::npos; p = arg.find(L'"', p + 2))
                    arg.replace(p, 1, L"\"\"");

                // surround with quotes
                arg.insert(arg.begin(), L'"');
                arg += '"';
            }
            else
                st += warg;
        }

        if (!st.empty())//first one does not need a preceeding space
            st += L' ';

    }
    return st;
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
    void _on_setup(Initializer &&init) {}

    template<typename Initializer> requires on_setup_init<Initializer>
    void _on_setup(Initializer &&init) { init.on_setup(*this); }

    template<typename Initializer>
    void _on_error(Initializer &&init) {}

    template<typename Initializer> requires on_error_init<Initializer>
    void _on_error(Initializer &&init) { init.on_error(*this, _ec); }

    template<typename Initializer>
    void _on_success(Initializer &&init) {}

    template<typename Initializer> requires on_success_init<Initializer>
    void _on_success(Initializer &&init) { init.on_success(*this); }

public:

    void set_error(const std::error_code & ec, const char* msg = "Unknown Error.")
    {
        _ec = ec;
        _error_msg = msg;
    }

    template<typename Args, typename ... Inits>
    auto launch(std::filesystem::path exe, Args && args_in, Inits && ... inits) -> PROCESS_NAMESPACE::process
    {
        cmd_line = build_args(exe, std::forward<Args>(args_in));

        (_on_setup(inits),...);

        if (_ec)
        {
            (_on_error(inits),...);
            throw process_error(_ec, _error_msg, exe);
        }

        PROCESS_NAMESPACE::process proc{};

        int launched_process = CreateProcessW(
                exe.c_str(),                                //       LPCSTR_ lpApplicationName,
                cmd_line.data(),                            //       LPSTR_ lpCommandLine,
                proc_attrs,                                 //       LPSECURITY_ATTRIBUTES_ lpProcessAttributes,
                thread_attrs,                               //       LPSECURITY_ATTRIBUTES_ lpThreadAttributes,
                inherit_handles,                            //       INT_ bInheritHandles,
                this->creation_flags,                       //       DWORD_ dwCreationFlags,
                reinterpret_cast<void*>(const_cast<wchar_t*>(env)),//LPVOID_ lpEnvironment,
                work_dir,                                   //       LPCSTR_ lpCurrentDirectory,
                &this->startup_info,                        //       LPSTARTUPINFOA_ lpStartupInfo,
                &this->proc_info);                          //       LPPROCESS_INFORMATION_ lpProcessInformation)

        proc._process_handle.proc_info = this->proc_info;

        if (launched_process != 0)
        {
            _ec.clear();
            (_on_success(inits),...);
        }
        else
        {
            _ec = get_last_error();
            _error_msg = "CreateProcess failed";
            (_on_error(inits),...);
            throw process_error(_ec, _error_msg, exe);
        }

        if (_ec)
        {
            (_on_error(inits),...);
            throw process_error(_ec, _error_msg, exe);
        }
        return proc;
    }

    LPSECURITY_ATTRIBUTES proc_attrs   = nullptr;
    LPSECURITY_ATTRIBUTES thread_attrs = nullptr;
    BOOL inherit_handles = false;
    const wchar_t * work_dir = nullptr;
    std::wstring cmd_line;
    const wchar_t * exe      = nullptr;
    const wchar_t * env      = nullptr;
    PROCESS_INFORMATION proc_info{nullptr, nullptr, 0,0};

    DWORD creation_flags = 0;

    STARTUPINFOEXW startup_info_ex
            {STARTUPINFOW {sizeof(STARTUPINFOW), nullptr, nullptr, nullptr,
                             0, 0, 0, 0, 0, 0, 0, 0, 0, 0, nullptr,
                             INVALID_HANDLE_VALUE,
                             INVALID_HANDLE_VALUE,
                             INVALID_HANDLE_VALUE},
             nullptr
            };
    STARTUPINFOW &startup_info = startup_info_ex.StartupInfo;
};

}

#endif