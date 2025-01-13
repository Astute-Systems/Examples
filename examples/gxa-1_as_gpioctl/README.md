# as-gpioctl

This GXA-1 GPIO control tool allows you to manage the GPIO pins on the GXA-1 using the gpiod library. It provides command-line options to turn the BIT LED on or off, display information about GPIO and devices, and control the wait time.

## Prerequisites

gpiod library: This tool uses the gpiod library to interact with GPIO pins. You need to have it installed on your system.

### Installing gpiod

On Debian-based systems (e.g., Ubuntu), you can install gpiod using:

``` .bash
sudo apt-get install gpiod
```

For other systems, refer to the libgpiod GitHub repository for installation instructions.

## Building the Tool

To build the tool, you need to have a C++ compiler and the gflags library installed.

### Installing gflags

On Debian-based systems, you can install gflags using:

``` .bash
sudo apt-get install libgflags-dev
```

For other systems, refer to the gflags GitHub repository for installation instructions.

### Compiling the Code

Use CMake to compile.

### Usage

``` .bash
./as-gpioctl [FLAGS]
```

### Available Flags

- ```--ledon```: Turn the BIT LED on.
- ```--ledoff```: Turn the BIT LED off.
- ```--quiet```: No additional output.
- ```--info```: Information on GPIO and devices.
- ```--wait=<seconds>```: Number of seconds to wait before releasing the GPIO line.

### Examples

Turn the BIT LED on:

``` .bash
./gpioctl --ledon
```

Turn the BIT LED off:

``` .bash
./gpioctl --ledon --wait=5
```

Wait for 5 seconds before releasing the GPIO line:

## License

This project is licensed under the MIT License. See the LICENSE file in the project root for full license details.

## References

- [https://github.com/brgl/libgpiod](libgpiod) GitHub repository
- [https://github.com/gflags/gflags](gflags) GitHub repository
  