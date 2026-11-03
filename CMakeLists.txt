cmake_minimum_required(VERSION 3.21 FATAL_ERROR)

project(RoTorrent
    VERSION 1.0.0
    DESCRIPTION "RoTorrent: a BitTorrent client built on libtorrent and Qt"
    LANGUAGES CXX
)

# RoTorrent is derived from qBittorrent and is distributed under GPL-2.0-or-later.
# See COPYING and docs/IDENTITY.md.

set(minBoostVersion 1.76)
if (NOT DEFINED minQt6Version)
    set(minQt6Version 6.6.0)
endif()
set(minOpenSSLVersion 3.0.2)
set(minLibtorrent1Version 1.2.19)
set(minLibtorrentVersion 2.0.10)
set(minZlibVersion 1.2.11)

list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake/Modules)

option(ROTORRENT_BUILD_GUI "Build the RoTorrent GUI application (needs Qt Widgets)" ON)
option(ROTORRENT_BUILD_TESTS "Build unit tests and the smoke test" OFF)
option(ROTORRENT_STACKTRACE "Capture a stack trace on a crash and show the Crash info dialog" ON)
option(ROTORRENT_BACKEND_ONLY "Configure only rotorrent_base (used for CI on non-Windows hosts)" OFF)

include(FeatureSummary)
include(CheckPackages)
include(CommonConfig)

# The version is set in ONE place: project(... VERSION x.y.z) above. ROTORRENT_VERSION_STATUS is the pre-release
# suffix ("" for a stable release, e.g. "rc1" or "beta2"), ROTORRENT_VERSION_BUILD the optional 4th number.
if (NOT DEFINED ROTORRENT_VERSION_STATUS)
    set(ROTORRENT_VERSION_STATUS "")
endif()
if (NOT DEFINED ROTORRENT_VERSION_BUILD)
    set(ROTORRENT_VERSION_BUILD 0)
endif()
configure_file("src/base/version.h.in" "${CMAKE_CURRENT_SOURCE_DIR}/src/base/version.h" @ONLY)

# Windows resource file (icon, VERSIONINFO, manifest). Configured once here so every Windows executable shares it;
# the version numbers come from project(VERSION) above.
if (WIN32)
    set(ROTORRENT_DIST_DIR "${CMAKE_CURRENT_SOURCE_DIR}/dist/windows")
    configure_file("${ROTORRENT_DIST_DIR}/rotorrent.rc.in" "${CMAKE_BINARY_DIR}/rotorrent.rc" @ONLY)
    set_source_files_properties("${CMAKE_BINARY_DIR}/rotorrent.rc" PROPERTIES
        OBJECT_DEPENDS "${ROTORRENT_DIST_DIR}/rotorrent.ico;${ROTORRENT_DIST_DIR}/rotorrent.manifest")
endif()

add_subdirectory(src)

if (ROTORRENT_BUILD_TESTS)
    enable_testing()
    add_subdirectory(test)
endif()
