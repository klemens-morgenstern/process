#include "popl.hpp"
#include <thread>
#include <filesystem>
#include <cstdlib>
int main(int argc, char** argv)
{
    popl::OptionParser op("Allowed options");

    auto exit_code = op.add<popl::Value<int>>("e", "exit-code", "Return this exit code", 0);
    auto wait_for = op.add<popl::Value<int>>("w", "wait", "Wait for this amount of milliseconds");
    auto std_err  = op.add<popl::Value<std::string>>("r", "err", "Print to stderr");
    auto std_out  = op.add<popl::Value<std::string>>("o", "out", "Print to stdout");
    auto std_in   = op.add<popl::Switch>("i", "in", "Write stdin to stdout");
    auto env      = op.add<popl::Value<std::string>>("v", "env", "Print env variable to stdout");
    auto cwd      = op.add<popl::Switch>("c", "cwd", "Print cwd to stdout");

    op.parse(argc, argv);

    if (wait_for->is_set())
        std::this_thread::sleep_for(std::chrono::milliseconds(wait_for->value()));

    if (std_err->is_set())
        std::cerr << std_err->value() << std::endl;

    if (std_out->is_set())
        std::cout << std_out->value() << std::endl;

    if (std_in->is_set())
    {
        std::string line;
        std::getline(std::cin, line);
        std::cout << line << std::endl;
    }

    if (cwd->is_set())
        std::cout << std::filesystem::current_path().string() << std::endl;

    if (env->is_set())
        std::cout << std::getenv(env->value().c_str()) << std::endl;


    return exit_code->value(); // the result from doctest is propagated here as well
}