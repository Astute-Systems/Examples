cmake_minimum_required(VERSION 3.10...3.20 FATAL_ERROR)

project(examples)

# Add all the executables to the bin directory
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

add_subdirectory(examples)
