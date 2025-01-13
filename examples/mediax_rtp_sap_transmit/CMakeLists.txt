project(rtp-sap-transmit)

find_package(PkgConfig REQUIRED)

pkg_check_modules(MEDIAX_STATIC REQUIRED mediax_static)
pkg_check_modules(MEDIAX REQUIRED mediax)

include_directories(${CMAKE_SOURCE_DIR}/src ${CMAKE_BINARY_DIR}/include ${MEDIAX_INCLUDE_DIRS})

# This line is use if we wish to compile with Gstreamer support enabled
add_definitions(-DGST_SUPPORTED)

# Transmit example linked against .so library
add_executable(rtp-sap-transmit rtp_sap_transmit.cc)
target_link_libraries(rtp-sap-transmit ${MEDIAX_LIBRARIES})

# Transmit example linked against .a library
add_executable(rtp-sap-transmit-static rtp_sap_transmit.cc)
target_link_libraries(rtp-sap-transmit-static ${MEDIAX_STATIC_LIBRARIES})