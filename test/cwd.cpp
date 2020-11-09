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

TEST_CASE("cwd_test")
{
    const auto tmp = std::filesystem::temp_directory_path() / "std_process_tmp_file";
    deleter d{tmp};


    proc::process p (std::filesystem::absolute(target_path), {"--cwd"}, proc::process_io{{}, tmp}, proc::process_start_dir(std::filesystem::temp_directory_path()));

    p.wait();
    CHECK(p.exit_code() == 0);

    std::ifstream ifs{tmp};
    CHECK(ifs);
    std::string line;

    CHECK(std::getline(ifs, line));
    CHECK(line == std::filesystem::temp_directory_path().string());
}

