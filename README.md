# Examples
![Version](https://astute-systems.github.io/Examples/version.svg)
[![Doxygen](https://github.com/Astute-Systems/Examples/actions/workflows/build-doxygen.yaml/badge.svg)](https://github.com/Astute-Systems/Examples/actions/workflows/build-doxygen.yaml)
[![Ubuntu 22.04 Intel](https://github.com/Astute-Systems/Examples/actions/workflows/build-ubuntu-22.04-amd64.yaml/badge.svg)](https://github.com/Astute-Systems/Examples/actions/workflows/build-ubuntu-22.04-amd64.yaml)

A collection of code examples for use by customer

| Example          | Description                                                 |
| ---------------- | ----------------------------------------------------------- |
| rtp-sap-transmit | MediaX example RTP video stream                             |
| capture_c        | GXA-1 capture analogue video example in C                   |
| capture_cpp      | GXA-1 capture analogue video example in C++ with SDL render |
| as-gpioctl       | GXA-1 example gpio control                                  |

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
