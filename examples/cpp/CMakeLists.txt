
# If running on github actions skip, check CI
if (NOT DEFINED ENV{GITHUB_ACTIONS})
    # add_subdirectory(mediax_rtp_sap_transmit)
endif()
add_subdirectory(common)
# add_subdirectory(zenoh)
add_subdirectory(gxa-1_capture_cpp)
add_subdirectory(gxa-1_capture_c)
add_subdirectory(gxa-1_as_gpioctl)
add_subdirectory(joystick_cpp)
add_subdirectory(gst-tank-overlay)
add_subdirectory(sdl_simple_render)
