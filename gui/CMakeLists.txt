cmake_minimum_required(VERSION 3.20)
project(ScarlettOS_GUI CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Subdirectories
add_subdirectory(compositor)
add_subdirectory(window_manager)
add_subdirectory(toolkit)
