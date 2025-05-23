#!/usr/bin/env python3
# -*- coding: utf-8 -*-
#
# Copyright (c) 2025, Advent Atum PTY LTD
#
# This file is part of the HORAS project developed by Advent Atum.
#
# See the commercial LICENSE file in the project directory for full license details.
#
# A simple example of using Zenoh to receive messages.

import sys
import os
import asyncio
import zenoh

topic = "horas/gnss/state"  # Topic to subscribe to

from datetime import datetime
sys.path.append(os.path.abspath(os.path.join(os.path.dirname(__file__), '..')))
from proto.gnss_pb2 import GNSSMessage

async def main():
    # Initialize Zenoh 
    config = zenoh.Config()

    # Declare a subscriber
    with zenoh.open(config) as session:
        # Subscribe to the topic
        def callback(sample):
            # Deserialize the message
            gnss_message = GNSSMessage()
            ## Allocate ZBytes to python string
            data_string = sample.payload.to_bytes()
            # Parse the received sample payload
            # The payload is a ZBytes, so we need to convert it to string
            gnss_message.ParseFromString(data_string)

            # double latitude = 1;  // Latitude in decimal degrees
            # double longitude = 2; // Longitude in decimal degrees

            # // Six-axis position information
            # float position_x = 3; // X-axis position in meters (e.g., East-West direction)
            # float position_y = 4; // Y-axis position in meters (e.g., North-South direction)
            # float position_z = 5; // Z-axis position in meters (e.g., altitude or depth)

            # // Orientation information (rotation around axes)
            # float rotation_x = 6; // Roll: Rotation around X-axis in degrees (tilt left/right)
            # float rotation_y = 7; // Pitch: Rotation around Y-axis in degrees (tilt forward/backward)
            # float rotation_z = 8; // Yaw (Heading): Rotation around Z-axis in degrees (compass direction)

            # // Barometric information
            # float pressure = 9;  // Atmospheric pressure in hPa

            # // G Force
            # float gforce = 10;    // G-force experienced by the system (unitless)

            # // Timestamp
            # uint64 timestamp = 11; // Timestamp of the message in milliseconds since Unix epoch

            # Print the message details in a nicely formatted way
            print(f"Received GNSS Message: {gnss_message}")
            print(f"Latitude: {gnss_message.latitude}")
            print(f"Longitude: {gnss_message.longitude}")
            print(f"Position X: {gnss_message.position_x}")
            print(f"Position Y: {gnss_message.position_y}")
            print(f"Position Z: {gnss_message.position_z}")
            print(f"Rotation X: {gnss_message.rotation_x}")
            print(f"Rotation Y: {gnss_message.rotation_y}")
            print(f"Rotation Z: {gnss_message.rotation_z}")
            print(f"Pressure: {gnss_message.pressure}")
            print(f"G-Force: {gnss_message.gforce}")
            print(f"Timestamp: {datetime.fromtimestamp(gnss_message.timestamp / 1000)}")
            print("---------------------------------------------------")
        
        # Subscribe to the topic "can0/tx/raw"
        session.declare_subscriber(topic, callback)
        print("Subscriber is ready. Waiting for messages...")

        # Keep the subscriber alive
        try:
            while True:
                # Sleep for a while to keep the subscriber alive
                await asyncio.sleep(1)
        except KeyboardInterrupt:
            print("Subscriber stopped.")

# Run the main function
if __name__ == "__main__":
    asyncio.run(main())
    print("Subscriber started. Waiting for messages...")