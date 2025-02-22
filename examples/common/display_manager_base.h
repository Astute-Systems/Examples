//
// Copyright (c) 2025, Astute Systems PTY LTD
//
// This file is part of the VivoeX project developed by Astute Systems.
//
// Licensed under the Attribution-NonCommercial 4.0 International (CC BY-NC 4.0)
// License. See the LICENSE file in the project root for full license details.
//
/// \file display_manager_base.h

#ifndef HARDWARE_DISPLAY_MANAGER_BASE_H_
#define HARDWARE_DISPLAY_MANAGER_BASE_H_

#include <errno.h>
#include <fcntl.h>
#include <linux/fb.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <string>
#include <vector>

/// Status enum
enum class Status { kSuccess, kFailure, kError };

/// The resolution struct
struct Resolution {
  /// \brief The width of the resolution
  int width;
  /// \brief The height of the resolution
  int height;
  /// \brief The bits per pixel
  int bpp;
};

/// The display manager class
class DisplayManagerBase {
 public:
  ///
  /// \brief Construct a new Display Manager object
  ///
  ///
  DisplayManagerBase() = default;

  ///
  /// \brief Destroy the Display Manager object
  ///
  ///
  virtual ~DisplayManagerBase() = default;

  ///
  /// \brief Initalise the display manager
  ///
  /// \return Status
  ///
  virtual Status Initalise() = 0;

  ///
  /// \brief The main event loop if there is one
  ///
  ///
  virtual void Run() = 0;

  ///
  /// \brief Stop the main loop if there is one
  ///
  ///
  virtual void Stop() {}

  ///
  /// \brief Get the Resolution attribute
  ///
  /// \return Resolution
  ///
  virtual Resolution GetResolution() = 0;

  ///
  /// \brief Buffer must be in the format RGBA
  ///
  /// \param frame_buffer the frame buffer to display
  /// \param resolution the resolution of the frame buffer
  /// \return Status
  ///
  virtual Status DisplayBuffer(uint8_t *frame_buffer, Resolution resolution, std::string text) = 0;

  ///
  /// \brief Rescale the video if needed
  ///
  /// \param frame_buffer The video buffer
  /// \param scaled_frame_buffer The scaled video buffer
  /// \param resolution The video buffer resolution
  /// \param height The required height
  /// \param width The required width
  /// \return Status Returns kSuccess if the video was scales, kFailure if no scaling required, kError if an error
  ///
  Status Rescale(uint8_t *frame_buffer, void *display_buffer, Resolution resolution, uint32_t height, uint32_t width);

  ///
  /// \brief Flush the framebuffer /dev/fb0
  ///
  ///
  virtual void Flush() = 0;

  ///
  /// \brief Toggle Fullscreen object
  ///
  ///
  virtual void ToggleFullscreen() {}

  /// \brief Last requested resolution
  Resolution last_requested_resolution_ = {0, 0, 0};

  /// \brief Scaled frame buffer for resolutions that do not match the display
  std::vector<uint8_t> scaled_frame_buffer_;
};

#endif  // HARDWARE_DISPLAY_MANAGER_BASE_H_
