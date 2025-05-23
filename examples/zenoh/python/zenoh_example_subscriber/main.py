#!/usr/bin/env python3
# -*- coding: utf-8 -*-
#
# Copyright (c) 2025,Astute Systems PTY LTD
#
# This file is part of the HORAS project developed byAstute Systems.
#
# See the commercial LICENSE file in the project directory for full license details.
#
# A simple example of using Zenoh to receive messages.

import sys
import os
import asyncio
import zenoh


from datetime import datetime
sys.path.append(os.path.abspath(os.path.join(os.path.dirname(__file__), '../../../../icd/proto/python')))
from common_can_pb2 import GNSSMessage

async def main():
    # Initialize Zenoh 
    config = zenoh.Config()

    # Declare a subscriber
    with zenoh.open(config) as session:
        # Subscribe to the topic
        def callback(sample):
            # Deserialize the message
            can_message = CANMessage()
            ## Allocate ZBytes to python string
            data_string = sample.payload.to_bytes()
            # Parse the received sample payload
            # The payload is a ZBytes, so we need to convert it to string
            can_message.ParseFromString(data_string)

            # Print the message details in a nicely formatted way
            print("\n==================== Received CAN Message ====================")
            print(f"ID:              {can_message.id:#X}")
            print(f"Data:            {list(can_message.data)}")
            print(f"Is Extended ID:  {can_message.is_extended_id}")
            print(f"Timestamp:       {datetime.fromtimestamp(can_message.timestamp / 1000)}")

        # Subscribe to the topic "can0/tx/raw"
        session.declare_subscriber("can0/tx/raw", callback)
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