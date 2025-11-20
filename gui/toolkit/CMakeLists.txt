cmake_minimum_required(VERSION 3.20)
project(Toolkit CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(toolkit STATIC
    src/widget.cpp
    src/button.cpp
    src/window.cpp
)

target_include_directories(toolkit PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

