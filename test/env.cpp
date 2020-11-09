#include "doctest.hpp"

#include <filesystem>
#include <process.hpp>

#include <iostream>
#include <fstream>

extern std::filesystem::path target_path;

struct deleter
{
    const std::filesystem::path & pt;
    deleter(const std::filesystem::path & pt) : pt(pt) {}
    ~deleter()
    {
        std::filesystem::remove(pt);
    }
};

TEST_CASE("env_test")
{
    const auto tmp = std::filesystem::temp_directory_path() / "std_process_tmp_file";
    deleter d{tmp};

#if defined(__unix__)
    proc::process p (target_path, {"--env", "FOO"}, proc::process_io{{}, tmp}, proc::process_env{{"FOO", "TEST-STRING"}});
#else
    proc::process p (target_path, {"--in"}, proc::process_io<FILE*, std::filesystem::path, FILE*>{stdout, tmp, stderr}, proc::process_env{{"FOO", "TEST-STRING"}});
#endif

    p.wait();
    CHECK(p.exit_code() == 0);

    std::ifstream ifs{tmp};
    CHECK(ifs);
    std::string line;

    CHECK(std::getline(ifs, line));
    CHECK(line == "TEST-STRING");
}

