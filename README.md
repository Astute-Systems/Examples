# Examples

A collection of code examples for use by customer

| Example               |  Description                             |
| --------------------- | ---------------------------------------- |
| rtp-sap-transmit      | MediaX example RTP video stream          |
| capture               | GXA-1 Capture analogue video example     |
| as-gpioctl            | GXA_1 example gpio control               |

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
