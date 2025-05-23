#!/bin/bash

# Generate a timestamp for the filename
TIMESTAMP=$(date +"%Y%m%d_%H%M%S")

gst-launch-1.0 v4l2src device=/dev/video0 ! \
          videoscale ! video/x-raw,width=720,height=576 ! \
          videoconvert ! \
          tee name=t ! \
          queue ! openh264enc bitrate=5000000 ! \
          h264parse ! tee name=stream_tee ! \
          queue ! matroskamux ! filesink location="output_${TIMESTAMP}.mkv" stream_tee. ! \
          queue ! rtph264pay config-interval=1 pt=96 ! udpsink host=255.255.255.255 port=5006