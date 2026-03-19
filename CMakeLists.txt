cmake_minimum_required(VERSION 3.20)

project(minirpc LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(CTest)
enable_testing()

find_package(Python3 REQUIRED COMPONENTS Interpreter)
find_package(Threads REQUIRED)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(MiniRpcCodegen)

set(MINIRPC_RUNTIME_SOURCES
    src/channel.cpp
    src/config.cpp
    src/protocol.cpp
    src/registry.cpp
    src/server.cpp
    src/thread_pool.cpp
)

add_library(minirpc STATIC ${MINIRPC_RUNTIME_SOURCES})
target_include_directories(minirpc PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_link_libraries(minirpc PUBLIC Threads::Threads)

mrpc_generate_cpp(
    CALCULATOR_GEN_SRCS
    CALCULATOR_GEN_HDRS
    "${CMAKE_CURRENT_SOURCE_DIR}/idl/calculator.mproto"
)

add_library(calculator_rpc STATIC ${CALCULATOR_GEN_SRCS})
target_include_directories(
    calculator_rpc
    PUBLIC
        "${CMAKE_CURRENT_BINARY_DIR}/generated"
)
target_link_libraries(calculator_rpc PUBLIC minirpc)

add_library(calculator_demo STATIC examples/calculator_service_impl.cpp)
target_include_directories(calculator_demo PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/examples")
target_link_libraries(calculator_demo PUBLIC calculator_rpc)

add_executable(calculator_server examples/calculator_server.cpp)
target_link_libraries(calculator_server PRIVATE calculator_demo)

add_executable(calculator_client examples/calculator_client.cpp)
target_link_libraries(calculator_client PRIVATE calculator_demo)

add_executable(test_runtime tests/test_runtime.cpp)
target_link_libraries(test_runtime PRIVATE minirpc)
add_test(NAME test_runtime COMMAND test_runtime)

add_executable(test_rpc_integration tests/test_rpc_integration.cpp)
target_link_libraries(test_rpc_integration PRIVATE calculator_demo)
add_test(NAME test_rpc_integration COMMAND test_rpc_integration)

add_executable(benchmark_rpc benchmarks/benchmark_rpc.cpp)
target_link_libraries(benchmark_rpc PRIVATE calculator_demo)
