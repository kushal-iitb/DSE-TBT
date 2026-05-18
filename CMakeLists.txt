cmake_minimum_required(VERSION 3.16)
project(DSE CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_BUILD_TYPE Release)

add_compile_options( -O3  -g -march=native -flto -Wall -Wextra)

add_subdirectory(third_party/quill)

add_executable(dse-tbt
    src/logging_object.cpp
    src/spsc_queue.cpp
    src/main.cpp
    src/multicast_publisher.cpp
    src/orderbook.cpp
    src/TcpHandler.cpp
    src/tick_history.cpp
)

target_include_directories(dse-tbt PRIVATE include)
target_link_libraries(dse-tbt PRIVATE quill::quill)


