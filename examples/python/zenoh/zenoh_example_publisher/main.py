#!/usr/bin/env python3
# -*- coding: utf-8 -*-
#
# Copyright (c) 2025,Astute Systems PTY LTD
#
# This file is part of the HORAS project developed byAstute Systems.
#
# See the commercial LICENSE file in the project directory for full license details.
#
# A simple example of using Zenoh to send and receive messages.

import sys
import os

# Add the parent directory of "common" to sys.path


import asyncio

import time
import zenoh
sys.path.append(os.path.abspath(os.path.join(os.path.dirname(__file__), '../../../../icd/proto/python')))
from common_can_pb2 import GNSSMessage


def main():
    # Initialize Zenoh
    config = zenoh.Config()
    with zenoh.open(config) as session:
        # Create a protobuf CAN message
        motor_message = MrMotor()
        motor_message.id = 10  # Set the message ID
        motor_message.velocity_target = 0.0  # Initial velocity

        t = 0.0
        dt = 0.04

        # Begin update loop
        target_velocity = 1.0
        for i in range(500):
            target_velocity += 0.01

            # Update the target velocity and round to 2 decimal places
            motor_message.velocity_target = round(target_velocity, 2)

            # Serialize the message to bytes
            serialized_message = motor_message.SerializeToString()

            # Publish the message
            session.put("can/tx/raw", serialized_message)

            # Print the current state
            print(f"Drive ID = {motor_message.id}, Velocity: {motor_message.velocity_target:.15f}")

            # Sleep for 10ms
            time.sleep(0.01)

        motor_message.velocity_target = 0
        # Serialize the message to bytes
        serialized_message = motor_message.SerializeToString()
        session.put("horas/motor/message", serialized_message)
        print("Message publishing completed.")


# Run the main function
if __name__ == "__main__":
    main()