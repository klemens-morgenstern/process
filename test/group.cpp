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
    auto pid1 = gp.emplace(target_path, {"--wait", "100", "--exit-code", "41"});
    auto pid2 = gp.emplace(target_path, {"--wait", "200", "--exit-code", "42"});
    CHECK(pid1);
    CHECK(gp.contains(pid1));
    CHECK(gp.contains(pid2));

    auto [pidex1, ex1] = gp.wait_one();
    CHECK(pid1 == pidex1);
    CHECK(41 == ex1);

    auto [pidex2, ex2] = gp.wait_one();
    CHECK(pid2 == pidex2);
    CHECK(42 == ex2);

    auto after = std::chrono::system_clock::now();
    CHECK(std::chrono::duration_cast<std::chrono::milliseconds>(after - before).count() >= 200);
}


TEST_CASE("async_wait_group")
{
    auto before = std::chrono::system_clock::now();
    proc::process_group gp;
    gp.emplace(target_path, {"--exit-code", "40", "--wait", "100"});

    auto after = before;
    bool did_something_else = false, done = false;

    asio::io_context ioc;
    gp.async_wait(ioc, [&](std::error_code ec) {

        if (ec)
            std::cerr << "Error: " << ec.message() << std::endl;

        CHECK(!ec);
        done = true;
        after = std::chrono::system_clock::now();
    });

    ioc.post([&]{did_something_else = true;});
    ioc.run();
    CHECK(std::chrono::duration_cast<std::chrono::milliseconds>(after - before).count() >= 100);
    CHECK(done);
    CHECK(did_something_else);
}



TEST_CASE("async_wait_group")
{
    auto before = std::chrono::system_clock::now();
    proc::process_group gp;
    auto pid1 = gp.emplace(target_path, {"--exit-code", "40", "--wait", "100"});
    auto pid2 = gp.emplace(target_path, {"--exit-code", "41", "--wait", "200"});

    auto after = before;
    auto after2 = before;
    bool did_something_else = false, done = false;

    asio::io_context ioc;
    gp.async_wait_one(ioc, [&](std::error_code ec, pid_t pid_, int exit_code) {
        CHECK(!ec);
        CHECK(pid1 == pid_);
        CHECK(exit_code == 40);
        after = std::chrono::system_clock::now();
        gp.async_wait_one(ioc,
            [&](std::error_code ec, pid_t pid_, int exit_code) {
                done = true;
                CHECK(!ec);
                CHECK(pid2 == pid_);
                CHECK(exit_code == 41);
                after2 = std::chrono::system_clock::now();
            });
    });

    ioc.post([&]{did_something_else = true;});
    ioc.run();
    CHECK(std::chrono::duration_cast<std::chrono::milliseconds>(after - before).count() >= 100);
    CHECK(std::chrono::duration_cast<std::chrono::milliseconds>(after2 - before).count() >= 200);
    CHECK(done);
    CHECK(did_something_else);
}