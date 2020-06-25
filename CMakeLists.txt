cmake_minimum_required(VERSION 3.0.2)

project(VirtualSpaceMeshnet)

# ZeroMQ setup
set(ZMQ_BUILD_TESTS OFF CACHE BOOL "")
set(WITH_DOCS OFF CACHE BOOL "")
set(ENABLE_DRAFTS ON CACHE BOOL "")
add_subdirectory(submodules/libzmq)

set(BUILD_TESTING OFF CACHE BOOL "")
set(CPPZMQ_BUILD_TESTS OFF CACHE BOOL "")
add_subdirectory(submodules/cppzmq)
target_compile_definitions(cppzmq INTERFACE ZMQ_BUILD_DRAFT_API=1)

# flatbuffers setup
set(FLATBUFFERS_BUILD_TESTS OFF CACHE BOOL "")
add_subdirectory(submodules/flatbuffers
  ${CMAKE_CURRENT_BINARY_DIR}/flatbuffers-build EXCLUDE_FROM_ALL)
#target_compile_definitions(flatbuffers INTERFACE FLATBUFFERS_TRACK_VERIFIER_BUFFER_SIZE)

# C++ setup
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED True)
SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wpedantic -Wshadow")

# add vsm library
add_library(vsm
  src/ego_sphere.cpp
  src/mesh_node.cpp
  src/peer_tracker.cpp
  src/zmq_transport.cpp
)
target_link_libraries(vsm PUBLIC cppzmq flatbuffers)
target_include_directories(vsm PUBLIC include)

# add tests
option(VSM_BUILD_TESTS "Enable the build of tests and samples." OFF)
set(VSM_BUILD_TESTS ${BUILD_TESTS} CACHE BOOL "Build the tests for VSM")

if(VSM_BUILD_TESTS)
  # Catch2 setup
  set(CATCH_BUILD_TESTING OFF CACHE BOOL "")
  add_subdirectory(submodules/Catch2)

  # add unit tests main
  add_library(catch2_main test/test_main.cpp)
  target_link_libraries(catch2_main PUBLIC Catch2)

  # add unit tests
  add_executable(tests
    test/test_logger.cpp
    test/test_mesh_node.cpp
    test/test_ego_sphere.cpp
    test/test_peer_tracker.cpp
    test/test_zmq_transport.cpp
  )
  target_link_libraries(tests PUBLIC catch2_main vsm)
endif()
