project(common)

## Set C++ standard
set(CMAKE_CXX_STANDARD 17)

set(SOURCES 
    display_manager_base.cc 
    display_manager_sdl.cc 
)

## PkgConfig fo libdrm
find_package(PkgConfig REQUIRED)

## SDL2 and image
find_package(SDL2 REQUIRED)

include_directories(${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_BINARY_DIR}/include ${CMAKE_BINARY_DIR}/src)
add_library(common STATIC ${SOURCES})
target_include_directories(common PRIVATE ${CMAKE_SOURCE_DIR}/src ${CMAKE_BINARY_DIR}/include ${MEDIAX_INCLUDE_DIRS} ${CMAKE_BINARY_DIR}/_deps/install/usr/local/include ${LIBDRM_INCLUDE_DIRS})
target_link_libraries(common SDL2::SDL2 -lSDL2_image)
