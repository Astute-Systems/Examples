# Debian cpack 
set(CPACK_GENERATOR "DEB")
set(CPACK_DEBIAN_PACKAGE_MAINTAINER "Ross Newman")
set(CPACK_PACKAGE_VERSION "${MAJOR_VERSION}.${MINOR_VERSION}.${PATCH_VERSION}")


set(CPACK_DEBIAN_PACKAGE_ARCHITECTURE arm64)
set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
set(CPACK_DEBIAN_PACKAGE_GENERATE_SHLIBS ON)
set(CPACK_DEBIAN_PACKAGE_GENERATE_SHLIBS_POLICY ">=")
set(CPACK_DEBIAN_PACKAGE_CONTROL_STRICT_PERMISSION TRUE)
set(CPACK_DEBIAN_PACKAGE_SECTION "libs")
set(CPACK_DEBIAN_PACKAGE_HOMEPAGE "https://astutesys.com")
set(CPACK_DEBIAN_PACKAGE_DESCRIPTION "Analog Video Capture Drivers for Astute Systems GXA-1")
set(CPACK_DEBIAN_PACKAGE_NAME "tw686x")
set(CPACK_DEBIAN_FILE_NAME "${CPACK_DEBIAN_PACKAGE_NAME}_${CPACK_PACKAGE_VERSION}_${CPACK_DEBIAN_PACKAGE_ARCHITECTURE}.deb")
set(CPACK_STRIP_FILES TRUE)

# Postinst script
set(CPACK_DEBIAN_PACKAGE_CONTROL_EXTRA
    "${CMAKE_CURRENT_SOURCE_DIR}/scripts/cmake/postinst"
)

# Disable error -Wno-dev
set(CMAKE_WARN_DEPRECATED OFF)

include(CPack)