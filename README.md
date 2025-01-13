# Examples

A collection of code examples for use by customer

| Example               |  Description                             |
| --------------------- | ---------------------------------------- |
| rtp-sap-transmit      | MediaX example RTP video stream          |
| capture               | GXA-1 capture analogue video example     |
| as-gpioctl            | GXA-1 example gpio control               |

## Building

To build a specific example, select a test from the table above and run

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
