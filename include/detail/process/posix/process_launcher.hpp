#ifndef DETAIL_PROCESS_POSIX_PROCESS_LAUNCHER_HPP
#define DETAIL_PROCESS_POSIX_PROCESS_LAUNCHER_HPP

#include <detail/process.hpp>
#include <functional>

namespace PROCESS_NAMESPACE::detail::process::posix {

template<typename Init, typename Launcher = default_process_launcher>
concept on_setup_init = requires(Init initializer, Launcher launcher) { {initializer.on_setup(launcher)}; };

template<typename Init, typename Launcher = default_process_launcher>
concept on_success_init = requires(Init initializer, Launcher launcher) { {initializer.on_success(launcher)}; };

template<typename Init, typename Launcher = default_process_launcher>
concept on_error_init = requires(Init initializer, Launcher launcher) { {initializer.on_error(launcher, std::error_code())}; };

template<typename Init, typename Launcher = default_process_launcher>
concept on_fork_error_init = requires(Init initializer, Launcher launcher) { {initializer.on_fork_error(launcher, std::error_code())}; };

template<typename Init, typename Launcher = default_process_launcher>
concept on_exec_setup_init = requires(Init initializer, Launcher launcher) { {initializer.on_exec_setup(launcher, std::error_code())}; };

template<typename Init, typename Launcher = default_process_launcher>
concept on_exec_error_init = requires(Init initializer, Launcher launcher) { {initializer.on_exec_error(launcher, std::error_code())}; };




class default_process_launcher {
protected:
    std::error_code _ec;
    const char * _error_msg = nullptr;

    template<typename Initializer>
    void on_setup(Initializer &&initializer) {}

    template<typename Initializer> requires on_setup_init<Initializer>
    void on_setup(Initializer &&init) { init.on_setup(*this); }

    template<typename Initializer>
    void on_error(Initializer &&initializer) {}

    template<typename Initializer> requires on_error_init<Initializer>
    void on_error(Initializer &&init) { init.on_error(*this, _ec); }

    template<typename Initializer>
    void on_success(Initializer &&initializer) {}

    template<typename Initializer> requires on_success_init<Initializer>
    void on_success(Initializer &&init) { init.on_success(*this); }

    template<typename Initializer>
    void on_fork_error(Initializer &&initializer) {}

    template<typename Initializer> requires on_fork_error_init<Initializer>
    void on_fork_error(Initializer &&init) { init.on_success(*this); }

    template<typename Initializer>
    void on_exec_setup(Initializer &&initializer) {}

    template<typename Initializer> requires on_exec_setup_init<Initializer>
    void on_exec_setup(Initializer &&init) { init.on_exec_setup(*this); }

    template<typename Initializer>
    void on_exec_error(Initializer &&initializer) {}

    template<typename Initializer> requires on_exec_error_init<Initializer>
    void on_exec_error(Initializer &&init) { init.on_exec_error(*this, _ec); }


    void _write_error(int sink, const std::string & msg)
    {
        int data[2] = {_ec.value(),static_cast<int>(msg.size())};
        while (::write(sink, &data[0], sizeof(int) *2) == -1)
        {
            auto err = errno;

            if (err == EBADF)
                return;
            else if ((err != EINTR) && (err != EAGAIN))
                break;
        }
        while (::write(sink, &msg.front(), msg.size()) == -1)
        {
            auto err = errno;

            if (err == EBADF)
                return;
            else if ((err != EINTR) && (err != EAGAIN))
                break;
        }
    }

    std::string _msg_buffer;

    void _read_error(int source)
    {
        int data[2];

        _ec.clear();
        int count = 0;
        while ((count = ::read(source, &data[0], sizeof(int) *2 ) ) == -1)
        {
            //actually, this should block until it's read.
            auto err = errno;
            if ((err != EAGAIN ) && (err != EINTR))
                set_error(std::error_code(err, std::system_category()), "Error read pipe");
        }
        if (count == 0)
            return  ;

        std::error_code ec(data[0], std::system_category());
        std::string msg(data[1], ' ');

        while (::read(source, &msg.front(), msg.size() ) == -1)
        {
            //actually, this should block until it's read.
            auto err = errno;
            if ((err == EBADF) || (err == EPERM))//that should occur on success, therefore return.
                return;
                //EAGAIN not yet forked, EINTR interrupted, i.e. try again
            else if ((err != EAGAIN ) && (err != EINTR))
                set_error(std::error_code(err, std::system_category()), "Error read pipe");
        }
        _msg_buffer = std::move(msg);
        set_error(ec, _msg_buffer.c_str());
    }

public:
    void set_error(const std::error_code & ec, const char* msg = "Unknown Error.")
    {
        _ec = ec;
        _error_msg = msg;
    }

    template<typename Args>
    auto prepare_args(const std::filesystem::path &exe, Args && args)
    {
        std::vector<char*> vec{args.size() + 2, nullptr};
        vec[0] = const_cast<char*>(exe.c_str());
        std::transform(std::begin(args), std::end(args), vec.begin() + 1, [](std::string_view sv){return const_cast<char*>(sv.data());});
        return vec;
    }

    template<typename Args, typename ... Inits>
    auto launch(const std::filesystem::path &exe, Args && args, Inits && ... inits) -> PROCESS_NAMESPACE::process
    {
        //arg store
        auto arg_store = prepare_args(exe, std::forward<Args>(args));
        cmd_line = arg_store.data();
        struct pipe_guard
        {
            int p[2];
            pipe_guard() : p{-1,-1} {}

            ~pipe_guard()
            {
                if (p[0] != -1)
                    ::close(p[0]);
                if (p[1] != -1)
                    ::close(p[1]);
            }
        };
        {
            pipe_guard p;

            if (::pipe(p.p) == -1)
                set_error(get_last_error(), "pipe(2) failed");
            else if (::fcntl(p.p[1], F_SETFD, FD_CLOEXEC) == -1)
                set_error(get_last_error(), "fcntl(2) failed");//this might throw, so we need to be sure our pipe is safe.

            if (!_ec)
                (on_setup(inits),...);

            if (_ec)
            {
                (on_error(inits),...);
                throw process_error(_ec, _error_msg, exe);
            }

            pid = ::fork();
            if (pid == -1)
            {
                set_error(get_last_error(), "fork() failed");
                (on_error(inits),...);
                (on_fork_error(inits),...);
                throw process_error(_ec, _error_msg, exe);
            }
            else if (pid == 0)
            {
                ::close(p.p[0]);
                (on_exec_setup(inits),...);

                ::execve(exe.c_str(), cmd_line, env);
                set_error(get_last_error(), "execve failed");

                _write_error(p.p[1], _error_msg);
                ::close(p.p[1]);

                _exit(EXIT_FAILURE);
                return {};
            }

            ::close(p.p[1]);
            p.p[1] = -1;
            _read_error(p.p[0]);

        }
        if (_ec)
        {
            (on_error(inits),...);
            throw process_error(_ec, _error_msg, exe);
        }
        PROCESS_NAMESPACE::process proc{pid};
        (on_success(inits),...);

        if (_ec)
        {
            (on_error(inits),...);
            throw process_error(_ec, _error_msg, exe);
        }

        return proc;
    }

    const char * exe      = nullptr;
    char *const* cmd_line = nullptr;
    char **env      = ::environ;
    pid_t pid = -1;
};

}


#endif 