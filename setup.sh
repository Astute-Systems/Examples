#!/bin/bash

# Check sudo
if [ "$EUID" -ne 0 ]
  then echo "Please run as root"
  exit
fi

sudo apt install cmake g++ make libgpiod 