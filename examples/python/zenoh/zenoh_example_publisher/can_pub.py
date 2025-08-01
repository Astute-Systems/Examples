#!/usr/bin/env python3
# -*- coding: utf-8 -*-
#
# Copyright (c) 2025, Astute Systems PTY LTD
#
# This file is part of the VivoeX project developed byAstute Systems.
#
# See the commercial LICENSE file in the project directory for full license details.
#
# A simple example of using Zenoh to send and receive messages.

import sys
import os

# Add the parent directory of "common" to sys.path


import asyncio
sys.path.append(os.path.abspath(os.path.join(os.path.dirname(__file__), '../../../../icd/proto/python')))
import zenoh
from datetime import datetime, timezone

topic = "can/tx/raw"

def main():
    # Initialize Zenoh 
    config = zenoh.Config()

    # Declare a subscriber
    with zenoh.open(config) as session:
        # Create a protobuf CAN message
        serialized_message = "RANGE".SerializeToString()
        # Publish the message
        session.put(topic, serialized_message)

# Run the main function
if __name__ == "__main__":
    main()
    print("Message published successfully.")