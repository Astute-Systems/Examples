//
// Copyright (c) 2025, Astute Systems PTY LTD
//
// This file is part of the VivoeX project developed by Astute Systems.
//
// Licensed under the Attribution-NonCommercial 4.0 International (CC BY-NC 4.0)
// License. See the LICENSE file in the project root for full license details.
//
/// \file display_manager_sdl.h

#ifndef HARDWARE_DISPLAY_MANAGER_SDL_H_
#define HARDWARE_DISPLAY_MANAGER_SDL_H_

// SDL2 Direct Rendering Manager
#include <SDL2/SDL.h>
#include <errno.h>
#include <fcntl.h>
#include <linux/fb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "display_manager_base.h"

/// The display manager class
class DisplayManager : public DisplayManagerBase {
 public:
  ///
  /// \brief Construct a new Display Manager object
  ///
  ///
  DisplayManager();

  ///
  /// \brief Destroy the Display Manager object
  ///
  ///
  ~DisplayManager();

  ///
  /// \brief Construct a new Display Manager object (deleted)
  ///
  ///
  DisplayManager(const DisplayManager &) = delete;

  ///
  /// \brief Delete the copy operator
  ///
  /// \return DisplayManager&
  ///
  DisplayManager &operator=(const DisplayManager &) = delete;

  ///
  /// \brief Initalise the display manager
  ///
  /// \return StatusDisplayManager
  Status Initalise() final;
  Status Initalise(uint32_t width, uint32_t height);

  ///
  /// \brief Get the Resolution attribute
  ///
  /// \return Resolution
  ///
  Resolution GetResolution() final;

  ///
  /// \brief Buffer must be in the format RGBA
  ///
  /// \param frame_buffer the frame buffer to display
  /// \param resolution the resolution of the frame buffer
  /// \return Status
  ///
  Status DisplayBuffer(uint8_t *frame_buffer, Resolution resolution, std::string text) final;

  ///
  /// \brief Flush the framebuffer /dev/fb0
  ///
  ///
  void Flush() final;

  ///
  /// \brief Toggle Fullscreen
  ///
  ///
  void ToggleFullscreen() final;

  ///
  /// \brief Run the main event loop
  ///
  void Run() final;

  ///
  /// \brief Stop the main loop if there is one
  ///
  ///
  void Stop() final;

  /// \brief The text to display
  static std::string text_;

 private:
  /// \brief Frame buffer device
  static std::vector<uint8_t> frame_buffer_;
  /// \brief The default width
  static uint32_t width_;
  /// \brief The default height
  static uint32_t height_;
  /// \brief Initalized flag
  bool initaliased_ = false;
  /// \brief The draw buffer
  static std::vector<uint8_t> draw_buffer_;
  /// \brief The SDL window
  SDL_Window *window_ = nullptr;
  /// \brief The SDL surface
  SDL_Surface *surface_ = nullptr;
  /// \brief The SDL renderer
  SDL_Renderer *renderer_ = nullptr;
  /// \brief The SDL texture
  static SDL_Texture *texture_;
  /// \brief The SDL texture rect
  SDL_Rect texr_ = {0, 0, 0, 0};
  /// \brief The SDL event loop
  static bool running_;
};

#endif  // HARDWARE_DISPLAY_MANAGER_SDL_H_
