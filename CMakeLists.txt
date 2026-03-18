cmake_minimum_required(VERSION 3.10)

# Set the project name
project(EOMM_Matchmaking_Rmxpv)

# Specify the source files
set(SOURCES
    src/main.cpp
    src/other_source.cpp
)

# Add the executable
add_executable(EOMM_Matchmaking_Rmxpv ${SOURCES})

# Set compiler flags (optional)
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED True)