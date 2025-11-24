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

# set(
#     SOURCES
#     ./main.cpp
#     ./impl/tcp_client.cpp
# )

add_library(${PROJECT_NAME} INTERFACE)

target_include_directories(${PROJECT_NAME} INTERFACE ${HEADERS} ${Boost_INCLUDE_DIRS})

