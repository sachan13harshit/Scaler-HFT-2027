cmake_minimum_required(VERSION 3.14)
project(hft_market_data CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -march=native -DNDEBUG")

# Fetch dependencies
include(FetchContent)
FetchContent_Declare(fmt GIT_REPOSITORY https://github.com/fmtlib/fmt.git GIT_TAG 10.1.1 GIT_SHALLOW TRUE)
FetchContent_Declare(json GIT_REPOSITORY https://github.com/nlohmann/json.git GIT_TAG v3.11.3 GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(fmt json)

find_package(Threads REQUIRED)
find_package(Boost REQUIRED)

include_directories(${Boost_INCLUDE_DIRS})

add_executable(publisher publisher.cpp)
target_link_libraries(publisher fmt::fmt Threads::Threads)

add_executable(consumer_shm consumer_shm.cpp)
target_link_libraries(consumer_shm fmt::fmt Threads::Threads)

add_executable(consumer_tcp consumer_tcp.cpp)
target_link_libraries(consumer_tcp fmt::fmt nlohmann_json::nlohmann_json Threads::Threads)

target_compile_options(publisher PRIVATE -Wall -Wextra)
target_compile_options(consumer_shm PRIVATE -Wall -Wextra)
target_compile_options(consumer_tcp PRIVATE -Wall -Wextra)
