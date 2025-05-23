#!/usr/bin/env python3
# -*- coding: utf-8 -*-
#
# Copyright (c) 2025, Advent Atum PTY LTD
#
# This file is part of the HORAS project developed by Advent Atum.
#
# See the commercial LICENSE file in the project directory for full license details.
#
# A simple example of using Zenoh to send and receive messages.

import sys
import os

# Add the parent directory of "common" to sys.path


import asyncio
sys.path.append(os.path.abspath(os.path.join(os.path.dirname(__file__), '..')))
import zenoh
from datetime import datetime, timezone



def clamp(value, min_value, max_value):
    if value < min_value:
        return min_value
    elif value > max_value:
        return max_value
    return value

def main():
    # Initialize Zenoh 
    topic = "horas/motors"
    motor_id = 1
    motor_speed = 1
    config = zenoh.Config()

    # Declare a subscriber
    with zenoh.open(config) as session:
        # Create a protobuf CAN message
        motor_speed = clamp(motor_speed, -1, 1)
        
        serialized_message = {
            "motor_id": motor_id,
            "motor_speed": motor_speed,
            "timestamp": datetime.now(timezone.utc).isoformat()
        }
        # Publish the message
        session.put(topic, serialized_message)

# Run the main function
if __name__ == "__main__":
    main()
    print("Message published successfully.")