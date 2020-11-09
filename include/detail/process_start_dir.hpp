#ifndef PROCESS_PROCESS_START_DIR_HPP
#define PROCESS_PROCESS_START_DIR_HPP

#include <detail/process/config.hpp>
#include <filesystem>

#if defined(__unix__)
#include <unistd.h>
#endif

namespace PROCESS_NAMESPACE
{

struct process_start_dir
{
    process_start_dir(const std::filesystem::path &s) : s_(s) {}

#if defined(__unix__)
    template <class PosixExecutor>
    void on_exec_setup(PosixExecutor&) const
    {
        ::chdir(s_.c_str());
    }

    template <class Executor>
    void on_setup(Executor& exec) const
    {
    }
#else
    template <class Executor>
    void on_setup(Executor& exec) const
    {
        exec.work_dir = s_.c_str();
    }
#endif

private:
    std::filesystem::path s_;
};

}

#endif //PROCESS_PROCESS_START_DIR_HPP
