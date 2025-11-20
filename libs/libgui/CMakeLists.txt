cmake_minimum_required(VERSION 3.20)
project(LibGUI CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(libgui STATIC
    src/graphics.cpp
    src/font.cpp
    src/theme.cpp
)

target_include_directories(libgui PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

