#!/bin/bash


# Generate a timestamp for the filename
TIMESTAMP=$(date +"%Y%m%d_%H%M%S")

gst-launch-1.0 v4l2src device=/dev/video0 ! \
          videoconvert ! \
          tee name=t ! \
          queue ! textoverlay text="Recording /dev/video0" valignment=top halignment=left font-desc="Sans, 24" ! xvimagesink sync=false t. ! \
          queue ! openh264enc bitrate=5000000 ! \
          h264parse ! \
          matroskamux ! \
          filesink location="output_${TIMESTAMP}.mkv"