#!/usr/bin/env python3
"""Receive frames from the shared memory and display them."""

import numpy as np
import multiprocessing as mp
import cv2
import time

def receive_frames(shared_array, width, height, event):
    """Receive frames from the shared memory and display them."""
    frame_size = width * height * 3
    while True:
        event.wait()  # Wait for the signal that a new frame is available
        frame = np.frombuffer(shared_array.get_obj(), dtype=np.uint8).reshape((height, width, 3))
        cv2.imshow('Frame', frame)
        if cv2.waitKey(1) & 0xFF == ord('q'):
            break
        event.clear()  # Clear the event

if __name__ == "__main__":
    width, height = 640, 480

    # Create a shared memory array
    shared_array = mp.Array('B', width * height * 3)
    event = mp.Event()

    # Start the process to receive frames
    process = mp.Process(target=receive_frames, args=(shared_array, width, height, event))
    process.start()

    # Keep the main process alive
    try:
        while True:
            time.sleep(1)
    except KeyboardInterrupt:
        process.terminate()
        process.join()
        cv2.destroyAllWindows()
