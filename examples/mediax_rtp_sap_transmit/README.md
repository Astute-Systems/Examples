# rtp-sap-transmit

You can build this against the static library .a for MediaX or the .so library.

To switch just modify the ```pkg_check_modules(MEDIAX_STATIC REQUIRED mediax_static)``` in the CMakeLists.txt file.

## Change Codec

To switch codec replace namespace i.e. to switch to uncompressed video.

``` .cpp
  mediax::RtpSapTransmit<mediax::rtp::h264::gst::open::RtpH264GstOpenPayloader> rtp(
      "238.192.1.1", 5004, "test-session-name", 640, 480, 25, "H264");
```

Use ```RGB24``` but could also be ```YUV422``` ```MONO8``` ```MONO16``` when uncompressed.

``` .cpp
  mediax::RtpSapTransmit<mediax::rtp::uncompressed::RtpUncompressedPayloader> rtp(
    "238.192.1.1", 5004, "test-session-name", 640, 480, 25, "RGB24");
```
