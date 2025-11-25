cmake_minimum_required(VERSION 3.20)
project(LibGUI C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 11)

add_library(libgui STATIC
    src/graphics.cpp
    src/font.cpp
    src/theme.cpp
    src/compositor_client.c
)

target_include_directories(libgui PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

