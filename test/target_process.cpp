#include "popl.hpp"
#include <thread>

int main(int argc, char** argv)
{
    popl::OptionParser op("Allowed options");

    auto exit_code = op.add<popl::Value<int>>("e", "exit-code", "Return this exit code", 0);
    auto wait_for = op.add<popl::Value<int>>("w", "wait", "Wait for this amount of milliseconds");

    op.parse(argc, argv);

    if (wait_for->is_set())
        std::this_thread::sleep_for(std::chrono::milliseconds(wait_for->value()));

    return exit_code->value(); // the result from doctest is propagated here as well
}