//
// Copyright (c) 2025, Astute Systems PTY LTD
//
// This file is part of the VivoeX project developed by Astute Systems.
//
// Licensed under the Attribution-NonCommercial 4.0 International (CC BY-NC 4.0)
// License. See the LICENSE file in the project root for full license details.
//

#include <chrono>
#include <ctime>
#include <iostream>
#include <zenoh/zenoh.hpp>

#include "common_can.pb.h"  // Generated Protobuf header

void callback(const zenoh::Sample &sample) {
  // Deserialize the message
  CANMessage can_message;
  if (!can_message.ParseFromArray(sample.payload.data(), sample.payload.size())) {
    std::cerr << "Failed to parse CAN message." << std::endl;
    return;
  }

  // Print the message details
  std::cout << "\n==================== Received CAN Message ====================" << std::endl;
  std::cout << "ID:              0x" << std::hex << can_message.id() << std::dec << std::endl;
  std::cout << "Data:            [";
  for (int i = 0; i < can_message.data_size(); ++i) {
    std::cout << "0x" << std::hex << static_cast<int>(can_message.data(i)) << std::dec;
    if (i < can_message.data_size() - 1) std::cout << ", ";
  }
  std::cout << "]" << std::endl;
  std::cout << "Is Extended ID:  " << (can_message.is_extended_id() ? "true" : "false") << std::endl;

  // Convert timestamp to human-readable format
  auto timestamp_ms = can_message.timestamp();
  std::time_t timestamp_s = timestamp_ms / 1000;
  std::tm *tm = std::gmtime(&timestamp_s);
  char time_buffer[100];
  std::strftime(time_buffer, sizeof(time_buffer), "%Y-%m-%d %H:%M:%S", tm);
  std::cout << "Timestamp:       " << time_buffer << "." << (timestamp_ms % 1000) << " UTC" << std::endl;
}

int main() {
  try {
    // Initialize Zenoh
    zenoh::Config config;
    auto session = zenoh::open(config);

    // Subscribe to the topic "can0/tx/raw"
    session.declare_subscriber("can0/tx/raw", callback);
    std::cout << "Subscriber is ready. Waiting for messages..." << std::endl;

    // Keep the subscriber alive
    while (true) {
      std::this_thread::sleep_for(std::chrono::seconds(1));
    }
  } catch (const std::exception &e) {
    std::cerr << "Error: " << e.what() << std::endl;
    return 1;
  }

  return 0;
}