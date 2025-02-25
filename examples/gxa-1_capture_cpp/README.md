# Capture example in C++ (With SDL Display)

For use with the video capture device on the GXA-1 mission computer.

Press <kbd>f</kbd> to switch to fullscreen

## PAL TV standard

Capture tool will calculat the recieved frames persecond and update the console once very second when you run the capture.

For interlaces frames (progressive) run:

```
./bin/capture_cpp -io_method 1 -device /dev/video0 -video_standard=PAL
Device: /dev/video0
IO Method: 1
Video Standard: PAL
Progressive video
FPS: 25
```

for alternating buffers (odd/even lines) run:

```
./bin/capture_cpp -io_method 1 -device /dev/video0 -video_standard=PAL -interlaced
Device: /dev/video0
IO Method: 1
Video Standard: PAL
Interlaced video
FPS: 50
```

 > NOTE: In interlaced mode each frame is scaled using swscale SWS_BILINEAR interpolation see [here](https://ffmpeg.org/doxygen/6.1/group__libsws.html). This is not a deinterlcing algorithm but designed for speed.

### Interlaced vs Progressive

Interlaced video @50 FPS

Progressing video @25 FPS

![Interlaced](../../images/PAL_Interlaced.png)

**Interlaced video using interpolation to 'fill in' the missing lines**

![Progressive](../../images/PAL_Progressive.png)

**Progressive video lines interleaved, no deinterlacing algorithm applied**

## NTSC TV standard

```
./bin/capture_cpp -io_method 1 -device /dev/video3 -video_standard=NTSC
```

## Gstreamer

With the new driver you can deinterlace using gstreamer using the pipeline below.

``` .bash
gst-launch-1.0 -v v4l2src device=/dev/video0  ! videoconvert ! deinterlace ! xvimagesink
```

For FPS:

``` .bash
gst-launch-1.0 -v v4l2src device=/dev/video0  ! videoconvert ! deinterlace ! fpsdisplaysink
```

## YouTube

Video showing PAL / NTSC and progressive and interlaces peformance can be found on YouTube

* [PAL/NTSC](https://www.youtube.com/watch?v=2ZZsYBmEw5c) on YouTube
* [Interlaced vs Progessive](https://www.youtube.com/watch?v=KeAOh9Jrm80) on YouTube
