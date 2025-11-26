cmake_minimum_required(VERSION 3.28.3)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

project(udp_tcp_forwarder VERSION 1.0)

find_package(Boost 1.83 REQUIRED COMPONENTS thread program_options)

set(
    SOURCES
    ./main.cpp
)

add_subdirectory(core)
add_subdirectory(impl)
add_subdirectory(spdlog)

add_executable(${PROJECT_NAME} ${SOURCES})

target_link_libraries(${PROJECT_NAME} PUBLIC core impl Boost::thread Boost::program_options)
