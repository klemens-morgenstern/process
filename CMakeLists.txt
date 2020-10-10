project(process)
cmake_minimum_required(VERSION 3.12)

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    set(CMAKE_CXX_STANDARD 20)
    set(CMAKE_CXX_FLAGS -fconcepts)
elseif (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    set(CMAKE_CXX_STANDARD 20)
    set(CMAKE_CXX_FLAGS -fconcepts)
elseif (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    set(CMAKE_CXX_FLAGS /std:c++latest)
endif()

add_definitions(-DPROCESS_NAMESPACE=proc)

include_directories(./include ./libs/asio/asio/include)

add_subdirectory(test)