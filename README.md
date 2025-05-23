# Examples

![Version](https://astute-systems.github.io/Examples/version.svg)
[![Doxygen](https://github.com/Astute-Systems/Examples/actions/workflows/build-doxygen.yaml/badge.svg)](https://github.com/Astute-Systems/Examples/actions/workflows/build-doxygen.yaml)
[![Ubuntu 22.04 Intel](https://github.com/Astute-Systems/Examples/actions/workflows/build-ubuntu-22.04-amd64.yaml/badge.svg)](https://github.com/Astute-Systems/Examples/actions/workflows/build-ubuntu-22.04-amd64.yaml)

A collection of code examples for use by customer

| Example           | Description                                                  |
| ----------------- | ------------------------------------------------------------ |
| gst-tank-overlay  | A Gstreamer (RTP H.264) reticle overlay for a sight          |
| gxa-1_as_gpioctl  | A gpiod example fo r the GXA-1                               |
| gxa-1_capture_c   | A V4L2 example in C for the GXA-1 (PAL/NTSC), no display     |
| gxa-1_capture_cpp | A V4L2 example in C++ for the GXA-1 (PAL/NTSC), SDL2 display |
| joystick_cpp      | A joystick examples tested with XBox 360 gamepad             |
| joystick_rs       | A joystick examples tested with XBox 360 gamepad             |
| mediax_rtp-sap-transmit  | MediaX example RTP video stream                       |
| python_shm        | Python send video over shared memory example                 |
| sdl_simple_render | A simple direct render example using SDL2                    |
| zenoh/cpp/zenoh_example_publisher      | C++ zenoh/protobuf example              |
| zenoh/cpp/zenoh_example_subscriber     | C++ zenoh/protobuf example              |
| zenoh/python/zenoh_example_publisher   | Python zenoh/protobuf example           |
| zenoh/python/zenoh_example_subscriber  | Python zenoh/protobuf example           |
| zenoh/rust/zenoh_example_publisher     | Rust zenoh/protobuf example             |
| zenoh/rust/zenoh_example_simple        | Rust zenoh/protobuf example             |
| zenoh/rust/zenoh_example_subscriber    | Rust zenoh/protobuf example             |

## Documentation

The doxygen documentation can be found on the public facing Github pages [here](https://astute-systems.github.io/Examples/).

## Building

To build a specific example, select a test from the table above and run. Run ```./scripts/init_build_machine.sh``` to satisfy the dependencies.

``` .bash
mkdir build && cd build
cmake ..
make <test_name>
```

i.e.

``` .bash
make rtp-sap-transmit
```

> NOTE: You will need to install relevant libraries to compile i.e. obtain MediaX libraries from Astute Systems.

## Links

- Astute Systems <https://astutesys.com/software>
- MediaX <https://astute-systems.github.io/MediaX>
- GXA-1 <https://astutesys.com/gxa-1>
