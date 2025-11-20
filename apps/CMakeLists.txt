cmake_minimum_required(VERSION 3.20)
project(ScarlettOS_Apps CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Subdirectories
add_subdirectory(desktop)
add_subdirectory(taskbar)
add_subdirectory(terminal)
