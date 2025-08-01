cmake_minimum_required(VERSION 3.10)
include(FindPkgConfig)

# set the project name
project(tank)

find_package(SDL2 REQUIRED)
find_package(PkgConfig REQUIRED)
find_package(gflags REQUIRED) # Add gflags

pkg_check_modules(GSTREAMER gstreamer-1.0)
pkg_check_modules(GSTREAMER_BASE gstreamer-video-1.0 gstreamer-app-1.0)
pkg_check_modules(CAIRO cairo)

include_directories(${CMAKE_SOURCE_DIR}/examples ${CMAKE_CURRENT_SOURCE_DIR}/..)

# add the executable
add_executable(tank src/tank.cc
../common/display_manager_base.cc
../common/display_manager_sdl.cc
)
message(STATUS "Includes dirs ${GSTREAMER_INCLUDES}")
target_link_libraries(tank ${GSTREAMER_LIBRARIES} ${GSTREAMER_BASE_LIBRARIES} ${CAIRO_LIBRARIES} SDL2::SDL2 -lSDL2_image gflags) # Link gflags
target_include_directories(tank PUBLIC ${GSTREAMER_INCLUDE_DIRS} ${GSTREAMER_BASE_INCLUDE_DIRS} ${CAIRO_INCLUDE_DIRS} ${SDL2_INCLUDE_DIRS} ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_BINARY_DIR}/include ${CMAKE_BINARY_DIR}/src ${CMAKE_BINARY_DIR}/_deps/install/usr/local/include)
