cmake_minimum_required(VERSION 3.0.2)

project(VirtualSpaceMeshnet)

# ZeroMQ setup
set(WITH_DOCS OFF CACHE BOOL "")
set(ZMQ_BUILD_TESTS OFF CACHE BOOL "")
add_subdirectory(submodules/libzmq)
include_directories(submodules/cppzmq)

# C++ setup
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED True)
SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wpedantic -Wshadow")

# add the executable
add_executable(node node.cpp)
target_link_libraries(node PUBLIC libzmq)
