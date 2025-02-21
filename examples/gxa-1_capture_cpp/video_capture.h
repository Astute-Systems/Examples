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
  void errno_exit(const std::string &s);
  int xioctl(int fd, int request, void *arg);
  void yuv422_to_rgb(const uint8_t *yuv, uint8_t *rgb, int width, int height);
  void process_image(const void *p, int frame);
  int read_frame(int count);
  void mainloop();
  void stop_capturing();
  void start_capturing();
  void uninit_device();
  void init_read(unsigned int buffer_size);
  void init_mmap();
  void init_userp(unsigned int buffer_size);
  void init_device();
  void close_device();
  void open_device();
  void set_video_standard(const std::string &video_standard);

  std::string dev_name;
  io_method io;
  int fd;
  std::vector<buffer> buffers;
  DisplayManager display;
  std::string video_standard;
};

#endif  // VIDEO_CAPTURE_H
