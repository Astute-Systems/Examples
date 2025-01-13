cmake_minimum_required(VERSION 3.10)

# Set the project name
project(as-gpioctl)

# Specify the C standard
set (CMAKE_CXX_STANDARD 17)

# Find the libftdi1 library
find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBGPIOD REQUIRED libgpiod)

# Include the libftdi1 headers
include_directories(gpiolib ${CMAKE_BINARY_DIR}/include ${LIBGPIOD_INCLUDE_DIRS})

# Add the executable
add_executable(as-gpioctl gpioctl.cc)

# GFlags
find_package(gflags REQUIRED)

# Link the libftdi1 library
target_link_libraries(as-gpioctl ${LIBGPIOD_LIBRARIES} ${GFLAGS_LIBRARIES})

