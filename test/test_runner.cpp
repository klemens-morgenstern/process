#define DOCTEST_CONFIG_IMPLEMENT
#include "doctest.hpp"

#include <filesystem>

std::filesystem::path target_path{};

using std::operator""sv;

int main(int argc, const char** argv)
{
    auto arg_end = argv + argc;
    auto it = std::find_if(argv, arg_end, [](auto s){return s == "--"sv;});
    if ((it + 1) >= arg_end)
    {
        std::cerr << "You must pass in the target_executable after -- " << std::endl;
        return 1;
    }

    target_path = *( it+1 );
#if defined(_WIN32)
    target_path += ".exe";
#endif
    if (!std::filesystem::exists(target_path))
    {
        std::cerr << "Target exectutable " << target_path << " does not exist." << std::endl;
        return 1;
    }

    doctest::Context context(argc - 2, argv);
    return context.run(); // run
}

