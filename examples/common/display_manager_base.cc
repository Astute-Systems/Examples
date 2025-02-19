//
// Copyright (c) 2025, Astute Systems PTY LTD
//
// This file is part of the VivoeX project developed by Astute Systems.
//
// Licensed under the Attribution-NonCommercial 4.0 International (CC BY-NC 4.0)
// License. See the LICENSE file in the project root for full license details.
//
/// \file display_manager_base.cc

#include "display_manager_base.h"

#include <iostream>

Status DisplayManagerBase::Rescale(uint8_t *frame_buffer, void *display_buffer, Resolution resolution, uint32_t height,
                                   uint32_t width) {
  if (frame_buffer == nullptr) {
    return Status::kError;
  }

  if (resolution.width != width || resolution.height != height) {
    if (last_requested_resolution_.width != resolution.width ||
        last_requested_resolution_.height != resolution.height) {
      last_requested_resolution_ = resolution;
    }

    // Scale the video to fit the screen if needed
    if ((resolution.height != height) || (resolution.width != width)) {
      // mediax::video::ColourSpaceCpu convert;
      // convert.ScaleToSizeRgba(resolution.height, resolution.width, frame_buffer, height, width,
      //                         reinterpret_cast<uint8_t *>(display_buffer));
    }
    return Status::kSuccess;
  }
  // No rescale was required
  return Status::kFailure;
}
