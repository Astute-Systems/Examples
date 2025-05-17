//
// Copyright (c) 2025, Astute Systems PTY LTD
//
// This file is part of the VivoeX project developed by Astute Systems.
//
// Licensed under the Attribution-NonCommercial 4.0 International (CC BY-NC 4.0)
// License. See the LICENSE file in the project root for full license details.
//

#include <iostream>
#include <thread>

#include "common/display_manager_sdl.h"

// Fill with RGB24 test pattern
void fill_test_pattern(unsigned char *data, int height, int width) { memset(data, 0xFF, height * width * 3); }

int main(int argc, char **argv) {
  DisplayManager dm;
  dm.Initalise(640, 480, "Test SDL simple");
  std::thread display_thread(&DisplayManager::Run, &dm);
  display_thread.detach();

  unsigned char data[640 * 480 * 3];
  Resolution res = {640, 480, 3};
  fill_test_pattern(data, 640, 480);

  while (true) {
    dm.DisplayBuffer(data, res, "Tank Overlay");
    // Sleep 40ms
    std::this_thread::sleep_for(std::chrono::milliseconds(40));
    std::cout << "Buffer updated...\n";
  }
}