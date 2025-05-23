cmake_minimum_required(VERSION 3.10…3.16)

set(EXCLUDE_FROM_ALL TRUE)
project(rtp-sap-transmit)

find_package(PkgConfig REQUIRED)

pkg_check_modules(MEDIAX_STATIC REQUIRED mediax_static)
pkg_check_modules(MEDIAX REQUIRED mediax)

include_directories(${CMAKE_SOURCE_DIR}/src ${CMAKE_BINARY_DIR}/include ${MEDIAX_INCLUDE_DIRS})

# This line is use if we wish to compile with Gstreamer support enabled
add_definitions(-DGST_SUPPORTED)

# Transmit example linked against .so library, exclude from all as it requires mediax to be installed
add_executable(rtp-sap-transmit rtp_sap_transmit.cc)

target_link_libraries(rtp-sap-transmit ${MEDIAX_LIBRARIES})

# Transmit example linked against .a library
add_executable(rtp-sap-transmit-static rtp_sap_transmit.cc)
target_link_libraries(rtp-sap-transmit-static ${MEDIAX_STATIC_LIBRARIES})

# Don't build automatically as part of all (mediax needs to be installed)
set_target_properties(rtp-sap-transmit PROPERTIES EXCLUDE_FROM_ALL TRUE)
set_target_properties(rtp-sap-transmit-static PROPERTIES EXCLUDE_FROM_ALL TRUE)
