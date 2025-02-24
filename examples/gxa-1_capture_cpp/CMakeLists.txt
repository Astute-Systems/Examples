cmake_minimum_required(VERSION 3.10…3.16)
project(capture_cpp)

include_directories(${CMAKE_SOURCE_DIR}/examples)
find_package(SDL2 REQUIRED)
find_package(PkgConfig REQUIRED)
find_package(gflags REQUIRED)

pkg_check_modules(LIBSWSCALE REQUIRED IMPORTED_TARGET libswscale)
include_directories(${SDL2_INCLUDE_DIRS})

add_executable(capture_cpp main.cc video_capture.cc)
target_link_libraries(capture_cpp common ${SDL2_LIBRARIES} gflags PkgConfig::LIBSWSCALE)
