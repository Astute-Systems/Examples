project(examples)

# Add default bin location
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

add_subdirectory(zenoh_example_publisher)
add_subdirectory(zenoh_example_subscriber)