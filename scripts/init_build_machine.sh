#!/bin/bash -e

# Check root
if [ "$(id -u)" != "0" ]; then
    echo "This script must be run as root" 1>&2
    exit 1
fi

# Build tools
apt-get install -y build-essential cmake
# Install SDL2 and SDL Image
apt-get install -y libsdl2-dev libsdl2-image-dev libgpiod-dev libgflags-dev libswscale-dev libsdl2-dev gstreamer1.0-dev libgstreamer-plugins-base1.0-dev libcairo2-dev gstreamer1.0-libav

echo "deb [trusted=yes] https://download.eclipse.org/zenoh/debian-repo/ /" | sudo tee -a /etc/apt/sources.list.d/zenoh.list > /dev/null
apt-get update
apt-get install -y zenoh