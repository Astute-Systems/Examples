project(joystick_cpp)

# SDL2

find_package(SDL2 REQUIRED)
include_directories(${SDL2_INCLUDE_DIRS})

add_executable(joystick_cpp main.cc)
target_link_libraries(joystick_cpp ${SDL2_LIBRARIES})
# Add the include directory for the joystick header
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../include)