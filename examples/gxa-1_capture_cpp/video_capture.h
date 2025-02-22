//
// Copyright (c) 2025, Astute Systems PTY LTD
//
// This file is part of the GXA-1 product developed by Astute Systems.
//
// Licensed under the MIT License. See the LICENSE file in the project root for full license
// details.
//
/// \brief A tool to manage the GPIO pins on the GXA-1
///
/// \file gpioctl.cc
///

#ifndef VIDEO_CAPTURE_H
#define VIDEO_CAPTURE_H

#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#include "common/display_manager_sdl.h"

#define CLEAR(x) memset(&(x), 0, sizeof(x))
#define WIDTH 720
#define HEIGHT 576
#define GRAB_NUM_FRAMES 100
#define BYTESPERPIXEL 3  // for color

typedef enum {
  IO_METHOD_READ,
  IO_METHOD_MMAP,
  IO_METHOD_USERPTR,
} io_method;

struct buffer {
  void *start;
  size_t length;
};

typedef struct {
  int stride;
  int width;
  int height;
} image_info_t;

class VideoCapture {
 public:
  VideoCapture(const std::string &device, io_method io, const std::string &video_standard);
  ~VideoCapture();
  void Start();
  void Stop();

 private:
  ///
  /// \brief Handle errors by printing a message and exiting
  ///
  /// \param s The error message to print
  ///
  void errno_exit(const std::string &s);

  ///
  /// \brief Perform an ioctl system call
  ///
  /// \param fd The file descriptor
  /// \param request The request code
  /// \param arg The argument for the request
  /// \return The result of the ioctl call
  ///
  int xioctl(int fd, int request, void *arg);

  ///
  /// \brief Convert YUV422 to RGB
  ///
  /// \param yuv The input YUV buffer
  /// \param rgb The output RGB buffer
  /// \param width The width of the image
  /// \param height The height of the image
  ///
  void yuv422_to_rgb(const uint8_t *yuv, uint8_t *rgb, int width, int height);

  ///
  /// \brief Process a captured image
  ///
  /// \param p The image buffer
  /// \param frame The frame number
  ///
  void process_image(const void *p, int frame);

  ///
  /// \brief Read a frame from the video device
  ///
  /// \param count The frame count
  /// \return 1 if a frame was read, 0 otherwise
  ///
  int read_frame(int count);

  ///
  /// \brief Main loop for capturing video
  ///
  void mainloop();

  ///
  /// \brief Stop capturing video
  ///
  void stop_capturing();

  ///
  /// \brief Start capturing video
  ///
  void start_capturing();

  ///
  /// \brief Uninitialize the video device
  ///
  void uninit_device();

  ///
  /// \brief Initialize the read method
  ///
  /// \param buffer_size The size of the buffer
  ///
  void init_read(unsigned int buffer_size);

  ///
  /// \brief Initialize the memory map
  ///
  void init_mmap();

  ///
  /// \brief Initialize the user pointer
  ///
  /// \param buffer_size The size of the buffer
  ///
  void init_userp(unsigned int buffer_size);

  ///
  /// \brief Initialize the video device
  ///
  void init_device();

  ///
  /// \brief Close the video device
  ///
  void close_device();

  ///
  /// \brief Open the video device
  ///
  void open_device();

  ///
  /// \brief Set the video standard
  ///
  /// \param video_standard The video standard to set (e.g., "PAL", "NTSC")
  ///
  void set_video_standard(const std::string &video_standard);

  std::string dev_name;
  io_method io;
  int fd;
  std::vector<buffer> buffers;
  DisplayManager display;
  std::string video_standard;
};

#endif  // VIDEO_CAPTURE_H
