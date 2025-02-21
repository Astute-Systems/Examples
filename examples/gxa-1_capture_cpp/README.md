# Capture example in C++ (With SDL Display)

For use with the video capture device on the GXA-1 mission computer.

## PAL TV standard

```
./bin/capture_cpp -io_method 1 -device /dev/video0 -video_standard=PAL
```

## NTSC TV standard

```
./bin/capture_cpp -io_method 1 -device /dev/video3 -video_standard=NTSC
```

