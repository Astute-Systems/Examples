cmake_minimum_required(VERSION 3.10)
include(FindPkgConfig)

# set the project name
project(simple_sdl)

find_package(SDL2 REQUIRED)
find_package(PkgConfig REQUIRED)

pkg_check_modules(GSTREAMER gstreamer-1.0)
pkg_check_modules(GSTREAMER_BASE gstreamer-video-1.0 gstreamer-app-1.0)
pkg_check_modules(CAIRO cairo)

include_directories(${CMAKE_SOURCE_DIR}/examples ${CMAKE_CURRENT_SOURCE_DIR}/..)

# add the executable
add_executable(simple_sdl main.cc
../common/display_manager_base.cc
../common/display_manager_sdl.cc
)
message(STATUS "Includes dirs ${GSTREAMER_INCLUDES}")
target_link_libraries(simple_sdl SDL2::SDL2 -lSDL2_image)
target_include_directories(simple_sdl PUBLIC ${SDL2_INCLUDE_DIRS})
