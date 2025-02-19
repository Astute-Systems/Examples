cmake_minimum_required(VERSION 3.10…3.16)

set(CMAKE_CXX_STANDARD 17)

project(capture)
include_directories(${CMAKE_SOURCE_DIR}/examples)
add_executable(capture video_capture.cc)
target_link_libraries(capture common)