//
// Copyright (c) 2025, Astute Systems PTY LTD
//
// This file is part of the GXA-1 product developed by Astute Systems.
//
// Licensed under the MIT License. See the LICENSE file in the project root for full license
// details.
//
///
/// \brief A tool to manage the GPIO pins on the GXA-1
///
/// \file gpioctl.cc
///

#include <ctype.h>
#include <gflags/gflags.h>
#include <gpiod.h>
#include <signal.h>
#include <unistd.h>

#include <iostream>

/// \brief Flag to turn the LED on
DEFINE_bool(ledon, false, "Turn the BIT LED on");
/// \brief Flag to turn the LED off
DEFINE_bool(ledoff, false, "Turn the bit LED off");
/// \brief Flag to turn off additional output
DEFINE_bool(quiet, false, "No additional output");
/// \brief Flag to get information on GPIO and devices
DEFINE_bool(info, false, "Information on GPIO and devices");
/// \brief Flag to wait for a number of seconds
DEFINE_int64(wait, 0, "Number of seconds to wait");

///
/// \brief Signal interrupt handler
///
/// \param signum The signal number
///
void signal_handler(int signum) {
  std::cerr << "Closing device\n";
  exit(signum);
}

#ifndef CONSUMER
/// \brief The consumer name
#define CONSUMER "Consumer"
#endif

/// \brief Main function
int main(int argc, char **argv) {
  std::cout << "GXA-1 GPIO control\n";

  // Register signal and signal handler
  signal(SIGINT, signal_handler);

  gflags::SetUsageMessage("Control the CBUS pins on the FTDI 232H chip");
  gflags::ParseCommandLineFlags(&argc, &argv, true);

  char chipname[] = "gpiochip0";
  unsigned int line_num = 96;  // GPIO Pin #23
  unsigned int val;
  struct gpiod_chip *chip;
  struct gpiod_line *line;
  int i, ret;

  if (FLAGS_ledon) {
    line_num = 96;
    val = 1;
  }

  if (FLAGS_ledoff) {
    line_num = 96;
    val = 0;
  }

  chip = gpiod_chip_open_by_name(chipname);
  if (!chip) {
    perror("Open chip failed\n");
    return -1;
  }

  line = gpiod_chip_get_line(chip, line_num);
  if (!line) {
    perror("Get line failed\n");
    return -2;
  }

  ret = gpiod_line_request_output(line, CONSUMER, 0);
  if (ret < 0) {
    perror("Request line as output failed\n");
    return -3;
  }

  ret = gpiod_line_set_value(line, val);
  if (ret < 0) {
    perror("Set line output failed\n");
  }

  sleep(FLAGS_wait);

  gpiod_line_release(line);
  gpiod_chip_close(chip);

  gflags::ShutDownCommandLineFlags();

  if (!FLAGS_quiet) std::cout << "Done!\n";

  return 0;
}
