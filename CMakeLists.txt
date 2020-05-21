cmake_minimum_required(VERSION 3.0.2)

project(VirtualSpaceMeshnet)

# ZeroMQ setup
set(BUILD_TESTS OFF CACHE BOOL "")
set(ZMQ_BUILD_TESTS OFF CACHE BOOL "")
set(WITH_DOCS OFF CACHE BOOL "")
add_subdirectory(submodules/libzmq)

set(BUILD_TESTING OFF CACHE BOOL "")
set(CPPZMQ_BUILD_TESTS OFF CACHE BOOL "")
set(ENABLE_DRAFTS ON CACHE BOOL "")
add_subdirectory(submodules/cppzmq)

# C++ setup
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED True)
SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wpedantic -Wshadow")

# add the executable
add_executable(node node.cpp)
target_link_libraries(node PUBLIC cppzmq)
