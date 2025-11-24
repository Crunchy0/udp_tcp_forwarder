cmake_minimum_required(VERSION 3.28.3)

project(impl VERSION 1.0)

find_package(Boost 1.83 REQUIRED COMPONENTS system)

set(
    HEADERS
    ./
    ./aux/include
    ./endpoints/include
)

set(
    SOURCES
    ./aux/source/edr_logger.cpp
    ./endpoints/source/tcp_client.cpp
)

add_library(${PROJECT_NAME} ${SOURCES})

target_include_directories(${PROJECT_NAME} PUBLIC ${HEADERS} PRIVATE ${Boost_INCLUDE_DIRS})
target_link_libraries(${PROJECT_NAME} PUBLIC core)
