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
#include <common/display_manager_sdl.h>
#include <errno.h>
#include <fcntl.h>   // low-level i/o
#include <getopt.h>  // getopt_long()
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
#include <libswscale/swscale.h>
#include <algorithm>  // for std::clamp

#include <cstdlib>
#include <iostream>
#include <thread>

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

static char *dev_name = NULL;
static io_method io = IO_METHOD_MMAP;
static int fd = -1;
struct buffer *buffers = NULL;
static unsigned int n_buffers = 0;

typedef struct {
  int stride;
  int width;
  int height;
} image_info_t;

DisplayManager display;

static void errno_exit(const char *s) {
  std::cerr << s << " error " << errno << ", " << strerror(errno) << std::endl;

  exit(EXIT_FAILURE);
}

static int xioctl(int fd, int request, void *arg) {
  int r;

  do r = ioctl(fd, request, arg);
  while (-1 == r && EINTR == errno);

  return r;
}

static void yuv422_to_rgb(const uint8_t* yuv, uint8_t* rgb, int width, int height) {
    int frameSize = width * height * 2;
    int rgbIndex = 0;

    for (int i = 0; i < frameSize; i += 4) {
        uint8_t y0 = yuv[i];
        uint8_t u = yuv[i + 1];
        uint8_t y1 = yuv[i + 2];
        uint8_t v = yuv[i + 3];

        int c = y0 - 16;
        int d = u - 128;
        int e = v - 128;

        int r = (298 * c + 409 * e + 128) >> 8;
        int g = (298 * c - 100 * d - 208 * e + 128) >> 8;
        int b = (298 * c + 516 * d + 128) >> 8;

        rgb[rgbIndex++] = std::clamp(r, 0, 255);
        rgb[rgbIndex++] = std::clamp(g, 0, 255);
        rgb[rgbIndex++] = std::clamp(b, 0, 255);

        c = y1 - 16;
        r = (298 * c + 409 * e + 128) >> 8;
        g = (298 * c - 100 * d - 208 * e + 128) >> 8;
        b = (298 * c + 516 * d + 128) >> 8;

        rgb[rgbIndex++] = std::clamp(r, 0, 255);
        rgb[rgbIndex++] = std::clamp(g, 0, 255);
        rgb[rgbIndex++] = std::clamp(b, 0, 255);
    }
}


static void process_image(const void *p, int frame) {
  image_info_t info;

  // set up the image save( or if SDL, display to screen)
  info.width = WIDTH;
  info.height = HEIGHT;
  info.stride = info.width * BYTESPERPIXEL;

  // Convert YUV422 to RGB
  uint8_t* rgb_buffer = (uint8_t*)malloc(WIDTH * HEIGHT * 3);
  yuv422_to_rgb((const uint8_t*)p, rgb_buffer, WIDTH, HEIGHT);

  Resolution res = {info.width, info.height, 3};
  display.DisplayBuffer(rgb_buffer, res, "Video Capture");

  free(rgb_buffer);
}

static int read_frame(int count) {
  struct v4l2_buffer buf;
  unsigned int i;

  switch (io) {
    case IO_METHOD_READ:
      if (-1 == read(fd, buffers[0].start, buffers[0].length)) {
        switch (errno) {
          case EAGAIN:
            return 0;

          case EIO:
            // Could ignore EIO, see spec.

            // fall through

          default:
            errno_exit("read");
        }
      }

      process_image(buffers[0].start, count);

      break;

    case IO_METHOD_MMAP:
      CLEAR(buf);

      buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
      buf.memory = V4L2_MEMORY_MMAP;

      if (-1 == xioctl(fd, VIDIOC_DQBUF, &buf)) {
        switch (errno) {
          case EAGAIN:
            return 0;

          case EIO:
            // Could ignore EIO, see spec.

            // fall through

          default:
            errno_exit("VIDIOC_DQBUF");
        }
      }

      assert(buf.index < n_buffers);

      process_image(buffers[buf.index].start, count);

      if (-1 == xioctl(fd, VIDIOC_QBUF, &buf)) errno_exit("VIDIOC_QBUF");

      break;

    case IO_METHOD_USERPTR:
      CLEAR(buf);

      buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
      buf.memory = V4L2_MEMORY_USERPTR;

      if (-1 == xioctl(fd, VIDIOC_DQBUF, &buf)) {
        switch (errno) {
          case EAGAIN:
            return 0;

          case EIO:
            // Could ignore EIO, see spec.

            // fall through

          default:
            errno_exit("VIDIOC_DQBUF");
        }
      }

      for (i = 0; i < n_buffers; ++i)
        if (buf.m.userptr == (unsigned long)buffers[i].start && buf.length == buffers[i].length) break;

      assert(i < n_buffers);

      process_image((void *)buf.m.userptr, count);

      if (-1 == xioctl(fd, VIDIOC_QBUF, &buf)) errno_exit("VIDIOC_QBUF");

      break;
  }

  return 1;
}

static void mainloop(void) {
  unsigned int count;

  count = GRAB_NUM_FRAMES;

  while (count-- > 0) {
    for (;;) {
      fd_set fds;
      struct timeval tv;
      int r;

      FD_ZERO(&fds);
      FD_SET(fd, &fds);

      // Timeout
      tv.tv_sec = 2;
      tv.tv_usec = 0;

      r = select(fd + 1, &fds, NULL, NULL, &tv);

      if (-1 == r) {
        if (EINTR == errno) continue;

        errno_exit("select");
      }

      if (0 == r) {
        std::cerr << "select timeout\n";
        exit(EXIT_FAILURE);
      }

      // if (read_frame(GRAB_NUM_FRAMES - count)) break;
      read_frame(GRAB_NUM_FRAMES - count);

      // EAGAIN - continue select loop.
    }
  }
}

static void stop_capturing(void) {
  enum v4l2_buf_type type;

  switch (io) {
    case IO_METHOD_READ:
      // Nothing to do.
      break;

    case IO_METHOD_MMAP:
    case IO_METHOD_USERPTR:
      type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

      if (-1 == xioctl(fd, VIDIOC_STREAMOFF, &type)) errno_exit("VIDIOC_STREAMOFF");

      break;
  }
}

static void start_capturing(void) {
  unsigned int i;
  enum v4l2_buf_type type;

  switch (io) {
    case IO_METHOD_READ:
      // Nothing to do.
      break;

    case IO_METHOD_MMAP:
      for (i = 0; i < n_buffers; ++i) {
        struct v4l2_buffer buf;

        CLEAR(buf);

        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index = i;

        if (-1 == xioctl(fd, VIDIOC_QBUF, &buf)) errno_exit("VIDIOC_QBUF");
      }

      type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

      if (-1 == xioctl(fd, VIDIOC_STREAMON, &type)) errno_exit("VIDIOC_STREAMON");

      break;

    case IO_METHOD_USERPTR:
      for (i = 0; i < n_buffers; ++i) {
        struct v4l2_buffer buf;

        CLEAR(buf);

        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_USERPTR;
        buf.m.userptr = (unsigned long)buffers[i].start;
        buf.length = buffers[i].length;

        if (-1 == xioctl(fd, VIDIOC_QBUF, &buf)) errno_exit("VIDIOC_QBUF");
      }

      type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

      if (-1 == xioctl(fd, VIDIOC_STREAMON, &type)) errno_exit("VIDIOC_STREAMON");

      break;
  }
}

static void uninit_device(void) {
  unsigned int i;

  switch (io) {
    case IO_METHOD_READ:
      free(buffers[0].start);
       break;

    case IO_METHOD_MMAP:
      for (i = 0; i < n_buffers; ++i)
        if (-1 == munmap(buffers[i].start, buffers[i].length)) errno_exit("munmap");
      break;

    case IO_METHOD_USERPTR:
      for (i = 0; i < n_buffers; ++i) free(buffers[i].start);
      break;
  }

  free(buffers);
}

static void init_read(unsigned int buffer_size) {
  buffers = (buffer *)calloc(1, sizeof(*buffers));

  if (!buffers) {
    std::cerr << "Out of memory\n";
    exit(EXIT_FAILURE);
  }

  buffers[0].length = buffer_size;
  buffers[0].start = malloc(buffer_size);

  if (!buffers[0].start) {
    std::cerr << "Out of memory\n";
    exit(EXIT_FAILURE);
  }
}

static void init_mmap(void) {
  struct v4l2_requestbuffers req;

  CLEAR(req);

  req.count = 4;
  req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  req.memory = V4L2_MEMORY_MMAP;

  if (-1 == xioctl(fd, VIDIOC_REQBUFS, &req)) {
    if (EINVAL == errno) {
      std::cerr << dev_name << " does not support memory mapping\n";
      exit(EXIT_FAILURE);
    } else {
      errno_exit("VIDIOC_REQBUFS");
    }
  }

  if (req.count < 2) {
    std::cerr << "Insufficient buffer memory on " << dev_name << "\n";
    exit(EXIT_FAILURE);
  }

  buffers = (buffer *)calloc(req.count, sizeof(*buffers));

  if (!buffers) {
    std::cerr << "Out of memory\n";
    exit(EXIT_FAILURE);
  }

  for (n_buffers = 0; n_buffers < req.count; ++n_buffers) {
    struct v4l2_buffer buf;

    CLEAR(buf);

    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;
    buf.index = n_buffers;

    if (-1 == xioctl(fd, VIDIOC_QUERYBUF, &buf)) errno_exit("VIDIOC_QUERYBUF");

    buffers[n_buffers].length = buf.length;
    buffers[n_buffers].start = mmap(NULL /* start anywhere */, buf.length, PROT_READ | PROT_WRITE /* required */,
                                    MAP_SHARED /* recommended */, fd, buf.m.offset);

    if (MAP_FAILED == buffers[n_buffers].start) errno_exit("mmap");
  }
}

static void init_userp(unsigned int buffer_size) {
  struct v4l2_requestbuffers req;

  CLEAR(req);

  req.count = 4;
  req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  req.memory = V4L2_MEMORY_USERPTR;

  if (-1 == xioctl(fd, VIDIOC_REQBUFS, &req)) {
    if (EINVAL == errno) {
      std::cerr << dev_name << " does not support user pointer i/o\n";
      exit(EXIT_FAILURE);
    } else {
      errno_exit("VIDIOC_REQBUFS");
    }
  }

  buffers = (buffer *)calloc(4, sizeof(*buffers));

  if (!buffers) {
    std::cerr << "Out of memory\n";
    exit(EXIT_FAILURE);
  }

  for (n_buffers = 0; n_buffers < 4; ++n_buffers) {
    buffers[n_buffers].length = buffer_size;
    buffers[n_buffers].start = malloc(buffer_size);

    if (!buffers[n_buffers].start) {
      std::cerr << "Out of memory\n";
      exit(EXIT_FAILURE);
    }
  }
}

static void init_device(void) {
  struct v4l2_capability cap;
  struct v4l2_cropcap cropcap;
  struct v4l2_crop crop;
  struct v4l2_format fmt;
  unsigned int min;
  int input, standard;

  if (-1 == xioctl(fd, VIDIOC_QUERYCAP, &cap)) {
    if (EINVAL == errno) {
      std::cerr << dev_name << " is no V4L2 device\n";
      exit(EXIT_FAILURE);
    } else {
      errno_exit("VIDIOC_QUERYCAP");
    }
  }

  if (!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE)) {
    std::cerr << dev_name << " is no video capture device\n";
    exit(EXIT_FAILURE);
  }

  switch (io) {
    case IO_METHOD_READ:
      if (!(cap.capabilities & V4L2_CAP_READWRITE)) {
        std::cerr << dev_name << " does not support read i/o\n";
        exit(EXIT_FAILURE);
      }

      break;

    case IO_METHOD_MMAP:
    case IO_METHOD_USERPTR:
      if (!(cap.capabilities & V4L2_CAP_STREAMING)) {
        std::cerr << dev_name << " does not support streaming i/o\n";
        exit(EXIT_FAILURE);
      }

      break;
  }

  // Select video input, video standard and tune here.

  // Reset Cropping
  std::cout << "...reset cropping of " << dev_name << " ..." << std::endl;
  CLEAR(cropcap);
  cropcap.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

  if (-1 == xioctl(fd, VIDIOC_CROPCAP, &cropcap)) {
    // Errors ignored.
  }

  crop.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  crop.c = cropcap.defrect;  // reset to default

  if (-1 == xioctl(fd, VIDIOC_S_CROP, &crop)) {
    switch (errno) {
      case EINVAL:
        // Cropping not supported.
        break;
      default:
        // Errors ignored.
        break;
    }
  }
  sleep(1);

  // Select standard
  std::cout << "...select standard of " << dev_name << " ..." << std::endl;
  // standard = V4L2_STD_NTSC;
  standard = V4L2_STD_PAL;
  if (-1 == xioctl(fd, VIDIOC_S_STD, &standard)) {
    perror("VIDIOC_S_STD");
    exit(EXIT_FAILURE);
  }
  sleep(1);

  // Select input
  std::cout << "...select input channel of " << dev_name << " ..." << std::endl << std::endl;
  input = 0;  // Composite-0
  if (-1 == ioctl(fd, VIDIOC_S_INPUT, &input)) {
    perror("VIDIOC_S_INPUT");
    exit(EXIT_FAILURE);
  }
  sleep(1);

  CLEAR(fmt);

  fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  fmt.fmt.pix.width = WIDTH;
  fmt.fmt.pix.height = HEIGHT;
  fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;
  if (BYTESPERPIXEL == 3)
    if (-1 == xioctl(fd, VIDIOC_S_FMT, &fmt)) errno_exit("VIDIOC_S_FMT");

  // Note VIDIOC_S_FMT may change width and height.

  // Buggy driver paranoia.
  min = fmt.fmt.pix.width * BYTESPERPIXEL;
  if (fmt.fmt.pix.bytesperline < min) fmt.fmt.pix.bytesperline = min;
  min = fmt.fmt.pix.bytesperline * fmt.fmt.pix.height;
  if (fmt.fmt.pix.sizeimage < min) fmt.fmt.pix.sizeimage = min;

  switch (io) {
    case IO_METHOD_READ:
      init_read(fmt.fmt.pix.sizeimage);
      break;

    case IO_METHOD_MMAP:
      init_mmap();
      break;

    case IO_METHOD_USERPTR:
      init_userp(fmt.fmt.pix.sizeimage);
      break;
  }
}

static void close_device(void) {
  if (-1 == close(fd)) errno_exit("close");

  fd = -1;
}

static void open_device(void) {
  struct stat st;

  if (-1 == stat(dev_name, &st)) {
    std::cerr << "Cannot identify '" << dev_name << "': " << errno << ", " << strerror(errno) << std::endl;
    exit(EXIT_FAILURE);
  }

  if (!S_ISCHR(st.st_mode)) {
    std::cerr << dev_name << " is no device\n";
    exit(EXIT_FAILURE);
  }

  fd = open(dev_name, O_RDWR /* required */ | O_NONBLOCK, 0);

  if (-1 == fd) {
    std::cerr << "Cannot open '" << dev_name << "': " << errno << ", " << strerror(errno) << std::endl;
    exit(EXIT_FAILURE);
  }
}

static void usage(FILE *fp, int argc, char **argv) {
  fprintf(fp,
          "Usage: %s [options]\n\n"
          "Options:\n"
          "-d | --device name   Video device name [/dev/video]\n"
          "-h | --help          Print this message\n"
          "-m | --mmap          Use memory mapped buffers\n"
          "-r | --read          Use read() calls\n"
          "-u | --userp         Use application allocated buffers\n"
          "\n",
          argv[0]);
}

static const char short_options[] = "d:hmru";

static const struct option long_options[] = {{"device", required_argument, NULL, 'd'}, {"help", no_argument, NULL, 'h'},
                                             {"mmap", no_argument, NULL, 'm'},         {"read", no_argument, NULL, 'r'},
                                             {"userp", no_argument, NULL, 'u'},        {0, 0, 0, 0}};

int main(int argc, char **argv) {
  dev_name = "/dev/video0";

  display.Initalise();
  
  // Run in thread display.Run()
  std::thread display_thread(&DisplayManager::Run, &display);

  // Test
  uint8_t *buf = (uint8_t *)malloc(720 * 576 * 4);

  for (int i = 0; i < 720 * 576 * 4; i++) buf[i] = 0xaa;
  Resolution res = {720, 576};
  display.DisplayBuffer((uint8_t *)buf, res, "Video Capture");

  for (;;) {
    int index;
    int c;

    c = getopt_long(argc, argv, short_options, long_options, &index);

    if (-1 == c) break;

    switch (c) {
      case 0:  // getopt_long() flag
        // break;
        {
          usage(stderr, argc, argv);
          exit(EXIT_FAILURE);
        }

      case 'd':
        dev_name = optarg;
        break;

      case 'h':
        usage(stdout, argc, argv);
        exit(EXIT_SUCCESS);

      case 'm':
        io = IO_METHOD_MMAP;
        break;

      case 'r':
        io = IO_METHOD_READ;
        break;

      case 'u':
        io = IO_METHOD_USERPTR;
        break;

      default:
        usage(stderr, argc, argv);
        exit(EXIT_FAILURE);
    }
  }

  open_device();

  std::cout << "...device opened..." << std::endl;
  init_device();

  std::cout << "...device initialized..." << std::endl;
  start_capturing();

  std::cout << "...device capturing..." << std::endl;
  mainloop();

  std::cout << "...device capturing stopped..." << std::endl;
  stop_capturing();

  std::cout << "...device capturing stopped..." << std::endl;
  uninit_device();

  std::cout << "...device uninitialized..." << std::endl;
  close_device();

  display.Stop();

  exit(EXIT_SUCCESS);

  return 0;
}
