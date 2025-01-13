//
// Copyright (c) 2025, Astute Systems PTY LTD
//
// This file is part of the VivoeX project developed by Astute Systems.
//
// Licensed under the Attribution-NonCommercial 4.0 International (CC BY-NC 4.0)
// License. See the LICENSE file in the project root for full license details.
//
/// \brief A SAP/SDP listener example
///
/// \file rtp_sap_transmit.cc
///

#include "rtp/rtp.h"

int main(int argc, char *argv[]) {
  mediax::RtpSapTransmit<mediax::rtp::h264::gst::open::RtpH264GstOpenPayloader> rtp(
      "238.192.1.1", 5004, "test-session-name", 640, 480, 25, "H264");
  std::vector<uint8_t> &data = rtp.GetBufferTestPattern(10);  // Bouncing ball
  while (true) rtp.Transmit(data.data(), false);
}
