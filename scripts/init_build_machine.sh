#!/bin/bash -e

# Check root
if [ "$(id -u)" != "0" ]; then
    echo "This script must be run as root" 1>&2
    exit 1
fi

# Build tools
apt-get install -y build-essential cmake
# Install SDL2 and SDL Image
apt-get install -y libsdl2-dev libsdl2-image-dev libgpiod-dev libgflags-dev
