cmake_minimum_required(VERSION 3.10…3.16)

project(capture_c LANGUAGES C)

set(CMAKE_C_COMPILER_INIT g++)

set_source_files_properties(video_capture.c PROPERTIES LANGUAGE C)
add_executable(capture_c video_capture.c)