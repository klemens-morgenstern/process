#include "doctest.hpp"

#include <filesystem>
#include <process.hpp>

#include <iostream>

extern std::filesystem::path target_path;


TEST_CASE("group_single")
{
    auto before = std::chrono::system_clock::now();
    proc::process_group gp;
    gp.emplace(target_path, {"--wait", "100"});
    gp.wait();
    auto after = std::chrono::system_clock::now();
    CHECK(std::chrono::duration_cast<std::chrono::milliseconds>(after - before).count() >= 100);
}

TEST_CASE("group_two")
{
    auto before = std::chrono::system_clock::now();
    proc::process_group gp;
    gp.emplace(target_path, {"--wait", "100"});
    gp.emplace(target_path, {"--wait", "200"});
    gp.wait();
    auto after = std::chrono::system_clock::now();
    CHECK(std::chrono::duration_cast<std::chrono::milliseconds>(after - before).count() >= 200);
}

TEST_CASE("group_two")
{
    auto before = std::chrono::system_clock::now();
    proc::process_group gp;
    gp.emplace(target_path, {"--wait", "100"});
    gp.emplace(target_path, {"--wait", "200"});
    gp.wait();
    auto after = std::chrono::system_clock::now();
    CHECK(std::chrono::duration_cast<std::chrono::milliseconds>(after - before).count() >= 200);
}


TEST_CASE("group_stepped")
{
    auto before = std::chrono::system_clock::now();
    proc::process_group gp;
    auto pid1 = gp.emplace(target_path, {"--wait", "100", "--exit-code", "1"});
    auto pid2 = gp.emplace(target_path, {"--wait", "200", "--exit-code", "2"});
    CHECK(pid1);
    CHECK(gp.contains(pid1));
    CHECK(gp.contains(pid2));

    auto [pidex1, ex1] = gp.wait_one();
    CHECK(pid1 == pidex1);
    CHECK(1 == ex1);

    auto [pidex2, ex2] = gp.wait_one();
    CHECK(pid2 == pidex2);
    CHECK(2 == ex2);

    auto after = std::chrono::system_clock::now();
    CHECK(std::chrono::duration_cast<std::chrono::milliseconds>(after - before).count() >= 200);
}