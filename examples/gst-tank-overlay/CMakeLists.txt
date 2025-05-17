cmake_minimum_required(VERSION 3.10)
include(FindPkgConfig)

# set the project name
project(tank)


pkg_check_modules(GSTREAMER gstreamer-1.0)
pkg_check_modules(GSTREAMER_BASE gstreamer-video-1.0 gstreamer-app-1.0)
pkg_check_modules(CAIRO cairo)

# add the executable
add_executable(tank src/tank.cc)
message(STATUS "Includes dirs ${GSTREAMER_INCLUDES}")
target_link_libraries(tank ${GSTREAMER_LIBRARIES} ${GSTREAMER_BASE_LIBRARIES} ${CAIRO_LIBRARIES})
target_include_directories(tank PUBLIC ${GSTREAMER_INCLUDE_DIRS} ${GSTREAMER_BASE_INCLUDE_DIRS} ${CAIRO_INCLUDE_DIRS})
