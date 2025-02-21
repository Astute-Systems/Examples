#include "video_capture.h"

#include <asm/types.h>  // for videodev2.h
#include <assert.h>
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
#include <sys/types.h>
#include <unistd.h>

#include <algorithm>
#include <iostream>
#include <stdexcept>
#include <thread>
#include <vector>

VideoCapture::VideoCapture(const std::string &device, io_method io, const std::string &video_standard)
    : dev_name(device), io(io), fd(-1), video_standard(video_standard) {
  // Set correct resolution based on standard
  if (video_standard == "NTSC") {
    height = 480;
    width = 720;
  } else if (video_standard == "PAL") {
    height = 576;
    width = 720;
  }

  display.Initalise(width, height, "Capture " + video_standard);
  std::thread display_thread(&DisplayManager::Run, &display);
  display_thread.detach();
  open_device();
  init_device();
  start_capturing();
}

VideoCapture::~VideoCapture() {
  stop_capturing();
  uninit_device();
  close_device();
  display.Stop();
}

void VideoCapture::Start() { mainloop(); }

void VideoCapture::Stop() { stop_capturing(); }

void VideoCapture::errno_exit(const std::string &s) {
  throw std::runtime_error(s + " error " + std::to_string(errno) + ", " + strerror(errno));
}

int VideoCapture::xioctl(int fd, int request, void *arg) {
  int r;

  do r = ioctl(fd, request, arg);
  while (-1 == r && EINTR == errno);

  return r;
}

void VideoCapture::yuv422_to_rgb(const uint8_t *yuv, uint8_t *rgb, int width, int height) {
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

void VideoCapture::process_image(const void *p) {
  image_info_t info;

  // set up the image save( or if SDL, display to screen)
  info.width = width;
  info.height = height;
  info.stride = info.width * BYTESPERPIXEL;

  // Convert YUV422 to RGB
  std::vector<uint8_t> rgb_buffer(width * height * 3);
  yuv422_to_rgb((const uint8_t *)p, rgb_buffer.data(), width, height);

  Resolution res = {info.width, info.height, 3};
  display.DisplayBuffer(rgb_buffer.data(), res, "Video Capture");
}

int VideoCapture::read_frame() {
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

      process_image(buffers[0].start);

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

      assert(buf.index < buffers.size());

      process_image(buffers[buf.index].start);

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

      for (i = 0; i < buffers.size(); ++i)
        if (buf.m.userptr == (unsigned long)buffers[i].start && buf.length == buffers[i].length) break;

      assert(i < buffers.size());

      process_image((void *)buf.m.userptr);

      if (-1 == xioctl(fd, VIDIOC_QBUF, &buf)) errno_exit("VIDIOC_QBUF");

      break;
  }

  return 1;
}

void VideoCapture::mainloop() {
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

    std::cout << "." << std::flush;

    read_frame();
    // EAGAIN - continue select loop.
  }
}

void VideoCapture::stop_capturing() {
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

void VideoCapture::start_capturing() {
  unsigned int i;
  enum v4l2_buf_type type;

  switch (io) {
    case IO_METHOD_READ:
      // Nothing to do.
      break;

    case IO_METHOD_MMAP:
      for (i = 0; i < buffers.size(); ++i) {
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
      for (i = 0; i < buffers.size(); ++i) {
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

void VideoCapture::uninit_device() {
  for (auto &buf : buffers) {
    switch (io) {
      case IO_METHOD_READ:
        free(buf.start);
        break;

      case IO_METHOD_MMAP:
        if (-1 == munmap(buf.start, buf.length)) errno_exit("munmap");
        break;

      case IO_METHOD_USERPTR:
        free(buf.start);
        break;
    }
  }
}

void VideoCapture::init_read(unsigned int buffer_size) {
  buffers.resize(1);

  buffers[0].length = buffer_size;
  buffers[0].start = malloc(buffer_size);

  if (!buffers[0].start) {
    throw std::runtime_error("Out of memory");
  }
}

void VideoCapture::init_mmap() {
  struct v4l2_requestbuffers req;

  CLEAR(req);

  req.count = 4;
  req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  req.memory = V4L2_MEMORY_MMAP;

  if (-1 == xioctl(fd, VIDIOC_REQBUFS, &req)) {
    if (EINVAL == errno) {
      throw std::runtime_error(dev_name + " does not support memory mapping");
    } else {
      errno_exit("VIDIOC_REQBUFS");
    }
  }

  if (req.count < 2) {
    throw std::runtime_error("Insufficient buffer memory on " + dev_name);
  }

  buffers.resize(req.count);

  for (int n_buffers = 0; n_buffers < req.count; ++n_buffers) {
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

void VideoCapture::init_userp(unsigned int buffer_size) {
  struct v4l2_requestbuffers req;

  CLEAR(req);

  req.count = 4;
  req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  req.memory = V4L2_MEMORY_USERPTR;

  if (-1 == xioctl(fd, VIDIOC_REQBUFS, &req)) {
    if (EINVAL == errno) {
      throw std::runtime_error(dev_name + " does not support user pointer i/o");
    } else {
      errno_exit("VIDIOC_REQBUFS");
    }
  }

  buffers.resize(4);

  for (int n_buffers = 0; n_buffers < 4; ++n_buffers) {
    buffers[n_buffers].length = buffer_size;
    buffers[n_buffers].start = malloc(buffer_size);

    if (!buffers[n_buffers].start) {
      throw std::runtime_error("Out of memory");
    }
  }
}

void VideoCapture::init_device() {
  struct v4l2_capability cap;
  struct v4l2_cropcap cropcap;
  struct v4l2_crop crop;
  struct v4l2_format fmt;
  unsigned int min;
  int input;

  if (-1 == xioctl(fd, VIDIOC_QUERYCAP, &cap)) {
    if (EINVAL == errno) {
      throw std::runtime_error(dev_name + " is no V4L2 device");
    } else {
      errno_exit("VIDIOC_QUERYCAP");
    }
  }

  if (!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE)) {
    throw std::runtime_error(dev_name + " is no video capture device");
  }

  switch (io) {
    case IO_METHOD_READ:
      if (!(cap.capabilities & V4L2_CAP_READWRITE)) {
        throw std::runtime_error(dev_name + " does not support read i/o");
      }

      break;

    case IO_METHOD_MMAP:
    case IO_METHOD_USERPTR:
      if (!(cap.capabilities & V4L2_CAP_STREAMING)) {
        throw std::runtime_error(dev_name + " does not support streaming i/o");
      }

      break;
  }

  // Select video input, video standard and tune here.

  // Reset Cropping
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
  set_video_standard(video_standard);
  sleep(1);

  // Select input
  input = 0;  // Composite-0
  if (-1 == ioctl(fd, VIDIOC_S_INPUT, &input)) {
    perror("VIDIOC_S_INPUT");
    exit(EXIT_FAILURE);
  }
  sleep(1);

  CLEAR(fmt);

  fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  fmt.fmt.pix.width = width;
  fmt.fmt.pix.height = height;
  fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;
  if (BYTESPERPIXEL == 2)
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

void VideoCapture::close_device() {
  if (-1 == close(fd)) errno_exit("close");

  fd = -1;
}

void VideoCapture::open_device() {
  struct stat st;

  if (-1 == stat(dev_name.c_str(), &st)) {
    throw std::runtime_error("Cannot identify '" + dev_name + "': " + std::to_string(errno) + ", " + strerror(errno));
  }

  if (!S_ISCHR(st.st_mode)) {
    throw std::runtime_error(dev_name + " is no device");
  }

  fd = open(dev_name.c_str(), O_RDWR /* required */ | O_NONBLOCK, 0);

  if (-1 == fd) {
    throw std::runtime_error("Cannot open '" + dev_name + "': " + std::to_string(errno) + ", " + strerror(errno));
  }
}

void VideoCapture::set_video_standard(const std::string &standard) {
  v4l2_std_id std_id;

  if (standard == "NTSC") {
    std_id = V4L2_STD_NTSC;
  } else if (standard == "PAL") {
    std_id = V4L2_STD_PAL;
  } else {
    throw std::runtime_error("Unsupported video standard: " + standard);
  }

  if (-1 == xioctl(fd, VIDIOC_S_STD, &std_id)) {
    perror("VIDIOC_S_STD");
    exit(EXIT_FAILURE);
  }
}
