cmake_minimum_required(VERSION 3.0.2)

project(VirtualSpaceMeshnet)

# ZeroMQ setup
set(ZMQ_BUILD_TESTS OFF CACHE BOOL "")
set(WITH_DOCS OFF CACHE BOOL "")
set(ENABLE_DRAFTS ON CACHE BOOL "")
add_subdirectory(submodules/libzmq)

set(BUILD_TESTING OFF CACHE BOOL "")
set(CPPZMQ_BUILD_TESTS OFF CACHE BOOL "")
ADD_DEFINITIONS (-DZMQ_BUILD_DRAFT_API=1)
add_subdirectory(submodules/cppzmq)

# flatbuffers setup
set(FLATBUFFERS_BUILD_TESTS OFF CACHE BOOL "")
set(CMAKE_SOURCE_DIR submodules/flatbuffers)
add_subdirectory(submodules/flatbuffers
  ${CMAKE_CURRENT_BINARY_DIR}/flatbuffers-build EXCLUDE_FROM_ALL)

# Catch2 setup
set(CATCH_BUILD_TESTING OFF CACHE BOOL "")
add_subdirectory(submodules/Catch2)

# C++ setup
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED True)
SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wpedantic -Wshadow")

# add the executable
add_executable(node src/node.cpp)
target_include_directories(node PUBLIC include)
target_link_libraries(node PUBLIC cppzmq flatbuffers)

# add tests
add_executable(tests test/test_main.cpp)
target_link_libraries(tests PUBLIC Catch2)
