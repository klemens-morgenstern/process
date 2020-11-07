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

TEST_CASE("stdout_path")
{
    const auto tmp = std::filesystem::temp_directory_path() / "std_process_tmp_file";
    deleter d{tmp};

    proc::process p (target_path, {"--out", "test string file"}, proc::process_io{.out = tmp});
    p.wait();
    REQUIRE(p.exit_code() == 0);

    std::ifstream ifs{tmp};
    REQUIRE(ifs);
    std::string line;

    REQUIRE(std::getline(ifs, line));
    REQUIRE(line == "test string file");
}


TEST_CASE("stderr_path")
{
    const auto tmp = std::filesystem::temp_directory_path() / "std_process_tmp_file";
    deleter d{tmp};

    proc::process p (target_path, {"--err", "test string file error"}, proc::process_io{.err = tmp});
    p.wait();
    REQUIRE(p.exit_code() == 0);

    std::ifstream ifs{tmp};
    REQUIRE(ifs);
    std::string line;

    REQUIRE(std::getline(ifs, line));
    REQUIRE(line == "test string file error");
}

TEST_CASE("stderr_path")
{
    const auto tmp    = std::filesystem::temp_directory_path() / "std_process_tmp_file";
    const auto tmp_in = std::filesystem::temp_directory_path() / "std_process_tmp_file_in";
    deleter d{tmp};
    deleter d_in{tmp_in};

    std::ofstream{tmp_in} << "some message sent to the the stream" << std::endl;
    proc::process p (target_path, {"--in"}, proc::process_io{.in = tmp_in, .out = tmp});
    p.wait();
    REQUIRE(p.exit_code() == 0);

    std::ifstream ifs{tmp};
    REQUIRE(ifs);
    std::string line;

    REQUIRE(std::getline(ifs, line));
    REQUIRE(line == "some message sent to the the stream");
}
