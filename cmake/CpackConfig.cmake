# Generate deb package
# CPack DEB documentation: https://cmake.org/cmake/help/latest/cpack_gen/deb.html
# Debian archive description: https://www.debian.org/doc/debian-policy/ch-archive.html
# Debian archive metadata: https://www.debian.org/doc/debian-policy/ch-controlfields.html

string(TIMESTAMP timestamp "%Y%m%d_%H-%M-%S")
set(CPACK_GENERATOR "DEB")
set(CPACK_STRIP_FILES TRUE)
set(CPACK_DEBIAN_PACKAGE_NAME bennu)

# "This variable may contain only alphanumerics (A-Za-z0-9) and the characters . + - ~ (full stop, plus, hyphen, tilde) and should start with a digit. If CPACK_DEBIAN_PACKAGE_RELEASE is not set then hyphens are not allowed."
# NOTE: "bennu_VERSION_WITH_COMMIT" gets set in the top-level CMakeLists.txt file
set(CPACK_DEBIAN_PACKAGE_VERSION ${bennu_VERSION_WITH_COMMIT})

# Build the deb for the architecture we are building on (amd64, arm64, ...)
execute_process(
  COMMAND dpkg --print-architecture
  OUTPUT_VARIABLE DEB_ARCH
  OUTPUT_STRIP_TRAILING_WHITESPACE
  ERROR_QUIET
)
if (NOT DEB_ARCH)
  if (CMAKE_SYSTEM_PROCESSOR MATCHES "^(aarch64|arm64)$")
    set(DEB_ARCH arm64)
  else ()
    set(DEB_ARCH amd64)
  endif ()
endif ()
set(CPACK_DEBIAN_PACKAGE_ARCHITECTURE ${DEB_ARCH})
set(CPACK_DEBIAN_PACKAGE_MAINTAINER "Sandia National Laboratories <emulytics@sandia.gov>")
set(CPACK_DEBIAN_PACKAGE_DEPENDS "libasio-dev, libboost-date-time-dev, libboost-filesystem-dev, libboost-program-options-dev, libboost-system-dev")
set(CPACK_DEBIAN_PACKAGE_SECTION misc)
set(CPACK_DEBIAN_PACKAGE_DESCRIPTION ${CMAKE_PROJECT_DESCRIPTION})
set(CPACK_PACKAGE_FILE_NAME "bennu-${CPACK_DEBIAN_PACKAGE_VERSION}_${DEB_ARCH}")
set(CPACK_DEBIAN_PACKAGE_HOMEPAGE ${CMAKE_PROJECT_HOMEPAGE_URL})

include(CPack)
