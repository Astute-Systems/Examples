//
// Copyright (c) 2025, Astute Systems PTY LTD
//
// This file is part of the VivoeX project developed by Astute Systems.
//
// Licensed under the Attribution-NonCommercial 4.0 International (CC BY-NC 4.0)
// License. See the LICENSE file in the project root for full license details.
//
/// \file display_manager_sdl.cc

// SDL2 includes

#include "display_manager_sdl.h"

#include <SDL2/SDL_image.h>
#include <signal.h>

#include <iostream>
#include <string>
#include <vector>

/// Static frame buffer
std::vector<uint8_t> DisplayManager::frame_buffer_;
SDL_Texture *DisplayManager::texture_ = nullptr;

/// Initalise width (default)
uint32_t DisplayManager::width_ = 0;
/// Initalise height (default)
uint32_t DisplayManager::height_ = 0;
/// Draw buffer
std::vector<uint8_t> DisplayManager::draw_buffer_;
bool DisplayManager::running_ = true;

std::string DisplayManager::text_ = "0x0";  // NOLINT

/// \brief Default width
#define DEFAULT_WIDTH 640
/// \brief Default height
#define DEFAULT_HEIGHT 480
/// \brief Default fullscreen
bool DEFAULT_FULLSCREEN = false;

DisplayManager::DisplayManager() {
  // Set the width and height
  width_ = DEFAULT_WIDTH;
  height_ = DEFAULT_HEIGHT;
  // This is the biggest it can possibly be
  draw_buffer_.resize(1920 * 1200 * 4);

  // Log the GTK Version
  // std::cout << "SDL2 Version " << SDL_MAJOR_VERSION << "." << SDL_MINOR_VERSION << "." << SDL_PATCHLEVEL << "\n";
}

DisplayManager::~DisplayManager() {
  running_ = false;
  SDL_Quit();
}
Status DisplayManager::Initalise() { return Initalise(DEFAULT_WIDTH, DEFAULT_HEIGHT, "Drivers Display SDL2"); }

Status DisplayManager::Initalise(uint32_t width, uint32_t height, std::string title) {
  // Set the width and height
  width_ = width;
  height_ = height;

  int fullscreen;
  if (DEFAULT_FULLSCREEN) {
    fullscreen = SDL_WINDOW_FULLSCREEN_DESKTOP;
  } else {
    fullscreen = 0;
  }

  // Initalise the SDL
  if (SDL_Init(SDL_INIT_VIDEO) < 0) {
    // If DISPLAY is not defined, then we can't use SDL2
    if (std::getenv("DISPLAY") == nullptr) {
      std::cerr << "DISPLAY is not defined, cannot use SDL2";
      return Status::kError;
    }
    std::cerr << "Unable to initalise SDL ("
              << "DISPLAY=" << std::getenv("DISPLAY") << "): " << SDL_GetError() << "\n";
    // List SDL2 modes
    int displayIndex = 0;
    int displayModeCount = SDL_GetNumDisplayModes(displayIndex);
    if (displayModeCount < 1) {
      std::cerr << "No display modes found\n";
      return Status::kError;
    } else {
      for (int i = 0; i < displayModeCount; i++) {
        SDL_DisplayMode mode = {SDL_PIXELFORMAT_UNKNOWN, 0, 0, 0, 0};
        SDL_GetDisplayMode(displayIndex, i, &mode);
        std::cout << "Display " << i << " " << mode.w << "x" << mode.h << " " << SDL_GetPixelFormatName(mode.format)
                  << "\n";
      }
    }

    return Status::kError;
  }

  // Create the window
  window_ =
      SDL_CreateWindow(title.c_str(), SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, width_, height_, fullscreen);

  if (window_ == nullptr) {
    std::cerr << "Unable to create window: " << SDL_GetError() << "\n";
    return Status::kError;
  }

  // Make window always on top
  SDL_SetWindowAlwaysOnTop(window_, SDL_TRUE);

  initaliased_ = true;
  return Status::kSuccess;
}

void DisplayManager::Run() {
  int w, h;  // texture width & height

  if (!window_) {
    std::cerr << "Window not created\n";
    exit(1);
  }

  renderer_ = SDL_CreateRenderer(window_, -1, SDL_RENDERER_ACCELERATED);

  if (!renderer_) {
    std::cerr << "Could not create renderer: " << SDL_GetError() << "\n";
    SDL_DestroyWindow(window_);  // Destroy the window before exiting
    window_ = nullptr;           // Set window_ to nullptr to avoid dangling pointer
    exit(1);
  }

  // Create the texture once
  texture_ = SDL_CreateTexture(renderer_, SDL_PIXELFORMAT_RGB24, SDL_TEXTUREACCESS_STREAMING, width_, height_);
  if (!texture_) {
    std::cerr << "Could not create texture: " << SDL_GetError() << "\n";
    SDL_DestroyRenderer(renderer_);
    SDL_DestroyWindow(window_);
    exit(1);
  }

  // Main loop
  SDL_Event event;
  while (SDL_WaitEvent(&event)) {
    if (event.type == SDL_QUIT)
      break;
    else if (event.type == SDL_KEYUP && event.key.keysym.sym == SDLK_ESCAPE)
      break;

    // Update the texture with the surface data
    SDL_UpdateTexture(texture_, NULL, draw_buffer_.data(), width_ * 3);

    // Clear the screen
    SDL_RenderClear(renderer_);

    // Get the width and height of the texture
    SDL_QueryTexture(texture_, NULL, NULL, &w, &h);

    // Check keypress if 'f' or 'F' is pressed, toggle fullscreen
    if (event.type == SDL_KEYUP && (event.key.keysym.sym == SDLK_f || event.key.keysym.sym == 'F')) {
      ToggleFullscreen();
    }

    if (DEFAULT_FULLSCREEN) {
      // If fullscreen, set the width and height to the screen's width and height
      Resolution res = GetResolution();
      h = res.height;
      w = res.width;
    } else {
      h = height_;
      w = width_;
    }

    texr_ = {0, 0, w, h};  // Rect to hold the texture's position and size

    // Copy the texture to the rendering context
    SDL_RenderCopy(renderer_, texture_, NULL, &texr_);

    // Flip the back buffer
    SDL_RenderPresent(renderer_);
  }

  if (texture_) SDL_DestroyTexture(texture_);
  if (renderer_) SDL_DestroyRenderer(renderer_);
  if (window_) SDL_DestroyWindow(window_);

  // Shutdown the application
  SDL_Quit();

  // Generate sigint to self to handle cleanup.
  raise(SIGINT);
}

void DisplayManager::Stop() {
  // Stop the SDL2 application
  SDL_QuitEvent event = {SDL_QUIT};
  SDL_PushEvent(reinterpret_cast<SDL_Event *>(&event));
}

void DisplayManager::ToggleFullscreen() {
  // TODO(ross.newman@defencex.ai): Check screen is not higher than 1920x1080 and error

  if (DEFAULT_FULLSCREEN) {
    // SDL2 windowed
    SDL_SetWindowFullscreen(window_, 0);
    DEFAULT_FULLSCREEN = false;
  } else {
    // Switch to fullscreen mode
    SDL_SetWindowFullscreen(window_, SDL_WINDOW_FULLSCREEN_DESKTOP);
    DEFAULT_FULLSCREEN = true;
  }
}

void DisplayManager::Flush() {
  // Flush the display
}

Resolution DisplayManager::GetResolution() {
  // Get the sdl window resolution
  Resolution resolution = {0, 0};

  if (window_ == nullptr) {
    std::cerr << "No window to get resolution from\n";
    return resolution;
  }

  // SDL2 window size
  int height;
  int width;
  SDL_GetWindowSize(window_, &width, &height);
  resolution.width = width;
  resolution.height = height;

  return resolution;
}

Status DisplayManager::DisplayBuffer(uint8_t *frame_buffer, Resolution resolution, std::string text) {
  text_ = text;
  if (!initaliased_) {
    std::cerr << "Display not initialised\n";
    return Status::kError;
  }

  if (frame_buffer == nullptr) {
    std::cerr << "No frame buffer to display\n";
    return Status::kError;
  }

  if (resolution.width != width_ || resolution.height != height_) {
    width_ = resolution.width;
    height_ = resolution.height;
    std::cerr << "Resolution changed to " << width_ << "x" << height_ << "\n";
  }

  memcpy(draw_buffer_.data(), frame_buffer, resolution.height * resolution.width * resolution.bpp);

  // Fill the mask area with black, last kMaskBottomPixels at the bottom of the screen
  draw_buffer_.resize((width_ * 3) * (height_));

  // Destroy the previous surface if it exists
  if (surface_) {
    SDL_FreeSurface(surface_);
    surface_ = nullptr;
  }

  // Create image from draw_buffer_.data()
  surface_ = SDL_CreateRGBSurfaceFrom(draw_buffer_.data() /*pixels*/, width_, height_, 24, (width_)*3, 0x000000FF,
                                      0x0000FF00, 0x00FF0000, 0);

  // SDL create event, this will cause the screen to refresh
  SDL_Event event = {};
  event.type = SDL_USEREVENT;
  SDL_PushEvent(&event);

  return Status::kSuccess;
}
