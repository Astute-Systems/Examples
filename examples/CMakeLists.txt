
# If running on github actions skip, check CI
if (NOT DEFINED ENV{GITHUB_ACTIONS})
    add_subdirectory(mediax_rtp_sap_transmit)
endif()
add_subdirectory(gxa-1_capture)
add_subdirectory(gxa-1_as_gpioctl)
