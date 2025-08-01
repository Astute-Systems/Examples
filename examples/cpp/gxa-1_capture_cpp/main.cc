//
// Copyright (c) 2025, Astute Systems PTY LTD
//
// This file is part of the VivoeX project developed by Astute Systems.
//
// Licensed under the Attribution-NonCommercial 4.0 International (CC BY-NC 4.0)
// License. See the LICENSE file in the project root for full license details.
//
/// \brief An example of how to capture video on the GXA-1
///
/// mplayer tv:// -tv driver=v4l2:norm=PAL:device=/dev/video0
///
/// \file video_capture.cc
///

#include <asm/types.h>  // for videodev2.h
#include <assert.h>
#include <errno.h>
#include <fcntl.h>   // low-level i/o
#include <getopt.h>  // getopt_long()
#include <libswscale/swscale.h>
#include <linux/videodev2.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#include <algorithm>  // for std::clamp
#include <cstdlib>
#include <iostream>
#include <string>
#include <thread>

#include "video_capture.h"

// Gflags
#include <gflags/gflags.h>

// Flag to set video device
DEFINE_string(device, "/dev/video0", "Video device name [/dev/video]");
// IO Method
DEFINE_int32(io_method, 1, "IO Method: 1 - MMAP, 2 - READ, 3 - USERPTR");
// Flag to set video standard
DEFINE_string(video_standard, "PAL", "Video standard [PAL, NTSC]");

int main(int argc, char **argv) {
  gflags::ParseCommandLineFlags(&argc, &argv, true);

  std::string device = FLAGS_device;
  io_method io = static_cast<io_method>(FLAGS_io_method);
  std::string video_standard = FLAGS_video_standard;

  // Print summary of flags
  std::cout << "Device: " << device << std::endl;
  std::cout << "IO Method: " << io << std::endl;
  std::cout << "Video Standard: " << video_standard << std::endl;

  try {
    VideoCapture capture(device, io, video_standard);
    capture.Start();
  } catch (const std::exception &e) {
    std::cerr << "Error: " << e.what() << std::endl;
    return EXIT_FAILURE;
  }

  // Keep thread alive
  while (true) {
    std::this_thread::sleep_for(std::chrono::seconds(1));
  }

  return EXIT_SUCCESS;
}
