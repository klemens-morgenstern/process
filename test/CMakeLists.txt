
add_library(compile_test compile_test.cpp)
add_executable(target_process target_process.cpp)

enable_testing()

add_executable(process_test test_runner.cpp wait_exit.cpp group.cpp)
add_dependencies(process_test target_process)

if (UNIX)
    set(THREADS_PREFER_PTHREAD_FLAG ON)
    find_package(Threads REQUIRED)
    target_link_libraries(process_test Threads::Threads)
endif (UNIX)

enable_testing()
add_test(NAME process_test COMMAND $<TARGET_FILE:process_test> -- $<TARGET_FILE:target_process>)