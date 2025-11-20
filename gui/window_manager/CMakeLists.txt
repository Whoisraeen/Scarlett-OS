cmake_minimum_required(VERSION 3.20)
project(WindowManager CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(window_manager
    src/main.cpp
    src/window_manager.cpp
)

target_include_directories(window_manager PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ../../kernel/include
)

