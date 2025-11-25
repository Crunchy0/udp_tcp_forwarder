cmake_minimum_required(VERSION 3.28.3)

project(core VERSION 1.0)

find_package(Boost 1.83 REQUIRED COMPONENTS json)

set(
    HEADERS
    ./
    ./aux
    ./endpoints
    ./scheduling
)

set(
    SOURCES
    ./aux/json_parser.cpp
)

add_library(${PROJECT_NAME} ${SOURCES})

target_include_directories(${PROJECT_NAME} PUBLIC ${HEADERS} ${Boost_INCLUDE_DIRS})
target_link_libraries(${PROJECT_NAME} PUBLIC Boost::json)

