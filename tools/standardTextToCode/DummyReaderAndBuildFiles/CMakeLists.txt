cmake_minimum_required(VERSION 3.10)
project(ParsingTestApp)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Collect all .cpp files in the current directory
file(GLOB SOURCES "*.cpp")

# Change to create a library instead of executable
# Remove the add_executable line and replace with:
add_library(ParsingTestApp ${SOURCES})
