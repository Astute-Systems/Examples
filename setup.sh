#!/bin/bash

# Check sudo
if [ "$EUID" -ne 0 ]
  then echo "Please run as root"
  exit
fi

apt install cmake g++ make libgpiod-dev 