#include "doctest.hpp"

#include <filesystem>
#include <process.hpp>

#include <iostream>

extern std::filesystem::path target_path;


TEST_CASE("exit-code")
{
    proc::process proc1{target_path, {"--exit-code", "42"}};
    proc1.wait();
    CHECK(!proc1.running());
    CHECK(proc1.exit_code() == 42);
}


TEST_CASE("wait")
{
    auto before = std::chrono::system_clock::now();
    proc::process proc1{target_path, {"--exit-code", "41", "--wait", "100"}};
    CHECK(proc1.id() > 0);
    CHECK(proc1.running());
    proc1.wait();
    auto after = std::chrono::system_clock::now();
    CHECK(!proc1.running());
    CHECK(proc1.exit_code() == 41);
    CHECK(std::chrono::duration_cast<std::chrono::milliseconds>(after - before).count() >= 100);
}

TEST_CASE("async_wait")
{
    auto before = std::chrono::system_clock::now();
    proc::process proc1{target_path, {"--exit-code", "40", "--wait", "100"}};
    CHECK(proc1.id() > 0);
    CHECK(proc1.running());

    auto after = before;
    bool did_something_else = false, done = false;

    asio::io_context ioc;
    proc1.async_wait(ioc, [&](std::error_code ec, int code) {

        if (ec)
            std::cerr << "Error: " << ec.message() << std::endl;

        CHECK(!ec);
        CHECK(code == 40);
        done = true;
        after = std::chrono::system_clock::now();
    });

    ioc.post([&]{did_something_else = true;});
    ioc.run();
    CHECK(!proc1.running());
    CHECK(std::chrono::duration_cast<std::chrono::milliseconds>(after - before).count() >= 100);
    CHECK(done);
    CHECK(did_something_else);
}