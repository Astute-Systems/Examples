#!/usr/bin/env python3
"""Send frames to the shared memory."""

import numpy as np
import multiprocessing as mp
import time

def create_red_frame(width, height):
    """Create a red frame of the given width and height."""
    frame = np.zeros((height, width, 3), dtype=np.uint8)
    frame[:, :, 0] = 255  # Set the red channel to 255
    return frame

def send_frames(shared_array, width, height, fps, event):
    """Send frames to the shared memory."""
    frame = create_red_frame(width, height)
    frame_size = width * height * 3
    while True:
        np.copyto(np.frombuffer(shared_array.get_obj(), dtype=np.uint8).reshape((height, width, 3)), frame)
        event.set()  # Signal that a new frame is available
        time.sleep(1 / fps)

if __name__ == "__main__":
    width, height = 640, 480
    fps = 25
    frame_size = width * height * 3

    # Create a shared memory array
    shared_array = mp.Array('B', frame_size)
    event = mp.Event()

    # Start the process to send frames
    process = mp.Process(target=send_frames, args=(shared_array, width, height, fps, event))
    process.start()

    # Keep the main process alive
    try:
        while True:
            time.sleep(1)
    except KeyboardInterrupt:
        process.terminate()
        process.join()