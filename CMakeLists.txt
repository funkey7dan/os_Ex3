cmake_minimum_required(VERSION 3.21)
project(Ex3)

set(CMAKE_CXX_STANDARD 23)

add_executable(Ex3 main.cpp)

find_package (Threads REQUIRED)

target_link_libraries(Ex3
        ${CMAKE_THREAD_LIBS_INIT}
        )
