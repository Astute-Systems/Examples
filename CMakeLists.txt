cmake_minimum_required(VERSION 3.10…3.16)

project(examples)

include(scripts/cmake/version.cmake)
include(scripts/cmake/cpack.cmake)

# Add all the executables to the bin directory
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

add_subdirectory(examples)

# If ZENOH_EXAMPLES defined
if (DEFINED ZENOH_EXAMPLES)
    message(STATUS "ZENOH_EXAMPLES defined")
    add_subdirectory(zenoh)
endif()

# if Arm architecture
message(STATUS "CMAKE_SYSTEM_PROCESSOR: ${CMAKE_SYSTEM_PROCESSOR}")
if (CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64")
    set(KERNEL_VERSION "5.15.148-tegra")

    set (MODULE_INSTALLDIR "/lib/modules/${KERNEL_VERSION}/kernel/drivers/media/video/tw686x")
    install (FILES ${CMAKE_SOURCE_DIR}/drivers/tw686x/tw6869.ko DESTINATION ${MODULE_INSTALLDIR})
    add_custom_target(analog ALL
        COMMAND ${CMAKE_COMMAND} -E chdir ${CMAKE_CURRENT_SOURCE_DIR}/drivers/tw686x make
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/drivers/tw686x
        COMMENT "Running Makefile")
endif()
