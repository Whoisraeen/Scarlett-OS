cmake_minimum_required(VERSION 3.20)
project(Compositor CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(compositor
    src/main.cpp
    src/compositor.cpp
)

target_include_directories(compositor PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ../../kernel/include
)

