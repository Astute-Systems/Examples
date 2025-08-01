project(zenoh_example_subscriber)
cmake_minimum_required(VERSION 3.10)

# Zenoh
find_package(zenoh REQUIRED)
# Protobuf
find_package(Protobuf REQUIRED)

add_executable(zenoh_example_subscriber
    main.cpp
)

target_link_libraries(zenoh_example_subscriber
    PRIVATE
        zenoh::zenoh
        Protobuf::Protobuf
)