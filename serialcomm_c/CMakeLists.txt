cmake_minimum_required(VERSION 3.15)

project(flexbot VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 14)
# set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# code runs the exact same on Linux and Windows
set(CMAKE_CXX_EXTENSIONS OFF) 

# This generates the compile_commands.json file
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_executable(flexbot_app
    src/serialcomm.cpp
    src/serialib.cpp 
)

# PRIVATE means these headers are only used by flexbot_app
target_include_directories(flexbot_app PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
