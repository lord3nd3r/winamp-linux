cmake_minimum_required(VERSION 3.16)
# Numeric version for CMake/CPack; full label lives in src/constants.h
# and must match the latest GitHub release tag (v1.0.0 → 1.0.0 / 1.0.0).
# C is required so Qt6's FindThreads dependency works on older CMake (e.g. Ubuntu 22.04 / Debian bookworm).
project(Winamp VERSION 1.0.0 LANGUAGES C CXX)
set(WINAMP_VERSION_FULL "1.0.0")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

set(QT_REQUIRED_COMPONENTS Core Gui Widgets Multimedia MultimediaWidgets OpenGL Network)

# Prefer Qt6, but support Qt5 as a fallback.
find_package(Qt6 QUIET COMPONENTS ${QT_REQUIRED_COMPONENTS} OpenGLWidgets Test)
if(Qt6_FOUND)
    set(QT_PACKAGE Qt6)
    set(QT_DBUS_FOUND_VAR Qt6DBus_FOUND)
    find_package(Qt6 REQUIRED COMPONENTS ${QT_REQUIRED_COMPONENTS} OpenGLWidgets Test)
    find_package(Qt6 COMPONENTS DBus QUIET)
    message(STATUS "Using Qt6")
else()
    set(QT_PACKAGE Qt5)
    set(QT_DBUS_FOUND_VAR Qt5DBus_FOUND)
    find_package(Qt5 REQUIRED COMPONENTS ${QT_REQUIRED_COMPONENTS} Test)
    find_package(Qt5 COMPONENTS DBus QUIET)
    message(STATUS "Using Qt5")
endif()

# DBus for MPRIS2 media player desktop integration (optional)
if(QT_PACKAGE STREQUAL "Qt6" AND ${QT_DBUS_FOUND_VAR})
    message(STATUS "Qt6 DBus found — MPRIS2 media player integration enabled")
elseif(QT_PACKAGE STREQUAL "Qt6")
    message(STATUS "Qt6 DBus not found — MPRIS2 disabled (media keys won't work)")
else()
    message(STATUS "Qt5 build selected — MPRIS2 integration is disabled on Qt5")
endif()

# projectM — Milkdrop-compatible visualization engine
# Install: sudo apt-get install libprojectm-dev projectm-data
find_library(PROJECTM_LIB projectM REQUIRED)

# TagLib — ID3v1/v2 + multi-format tag read/write (Winamp 2.x File Info)
find_package(PkgConfig REQUIRED)
pkg_check_modules(TAGLIB REQUIRED IMPORTED_TARGET taglib)

add_executable(winamp
    src/main.cpp
    src/winamp_window.h
    src/winamp_bitmaps.h
    src/compat.h
    src/eq_dsp.h
    src/constants.h
    src/translator.h
    src/bookmark_manager.h
    src/recent_files.h
    src/dialogs.h
    src/tag_metadata.h
    src/tag_metadata.cpp
    src/preferences.h
    src/preferences.cpp
    src/modern_skin.h
    src/playlist.h
    src/playlist.cpp
    src/equalizer.h
    src/equalizer.cpp
    src/video.h
    src/video.cpp
    src/milkdrop.h
    src/media_library.h
    src/mpris2_adaptors.h
    src/python_plugin.h
    src/python_plugin.cpp
)

# Compiler warnings
target_compile_options(winamp PRIVATE
    -Wall -Wextra -Wpedantic
    -Wno-unused-parameter
)

# Hardening flags
target_compile_options(winamp PRIVATE
    -fstack-protector-strong
)
target_compile_definitions(winamp PRIVATE _FORTIFY_SOURCE=2)

# Python plugins run in an out-of-process host (python3 + JSON-RPC).
# No embedded interpreter / pybind11 link required.
find_package(Python3 COMPONENTS Interpreter REQUIRED)

target_link_libraries(winamp
    ${QT_PACKAGE}::Core
    ${QT_PACKAGE}::Gui
    ${QT_PACKAGE}::Widgets
    ${QT_PACKAGE}::Multimedia
    ${QT_PACKAGE}::MultimediaWidgets
    ${QT_PACKAGE}::OpenGL
    ${QT_PACKAGE}::Network
    ${PROJECTM_LIB}
    PkgConfig::TAGLIB
)

if(QT_PACKAGE STREQUAL "Qt6")
    target_link_libraries(winamp
        Qt6::OpenGLWidgets
    )
endif()

if(QT_PACKAGE STREQUAL "Qt6" AND ${QT_DBUS_FOUND_VAR})
    target_link_libraries(winamp
        ${QT_PACKAGE}::DBus
    )
    target_compile_definitions(winamp PRIVATE QT_DBUS_LIB)
endif()

install(TARGETS winamp RUNTIME DESTINATION bin)
install(FILES winamp.desktop DESTINATION share/applications)
install(DIRECTORY skins/ DESTINATION share/winamp/skins)
install(DIRECTORY assets/ DESTINATION share/winamp/resource)
install(DIRECTORY lang/ DESTINATION share/winamp/lang)

# Testing
enable_testing()

add_executable(winamp_tests
    tests/test_eq_dsp.cpp
)
target_link_libraries(winamp_tests PRIVATE
    ${QT_PACKAGE}::Core
    ${QT_PACKAGE}::Test
)
add_test(NAME eq_dsp_test COMMAND winamp_tests)

add_executable(playlist_tests
    tests/test_playlist.cpp
    src/playlist.cpp
    src/equalizer.cpp
    src/preferences.cpp
    src/video.cpp
    src/tag_metadata.cpp
    src/dialogs.h
)
target_link_libraries(playlist_tests PRIVATE
    ${QT_PACKAGE}::Core
    ${QT_PACKAGE}::Gui
    ${QT_PACKAGE}::Widgets
    ${QT_PACKAGE}::Multimedia
    ${QT_PACKAGE}::MultimediaWidgets
    ${QT_PACKAGE}::OpenGL
    ${QT_PACKAGE}::Network
    ${QT_PACKAGE}::Test
    PkgConfig::TAGLIB
)
if(QT_PACKAGE STREQUAL "Qt6")
    target_link_libraries(playlist_tests PRIVATE Qt6::OpenGLWidgets)
endif()
add_test(NAME playlist_test COMMAND playlist_tests)

add_executable(tag_metadata_tests
    tests/test_tag_metadata.cpp
    src/tag_metadata.cpp
)
target_link_libraries(tag_metadata_tests PRIVATE
    ${QT_PACKAGE}::Core
    ${QT_PACKAGE}::Test
    PkgConfig::TAGLIB
)
add_test(NAME tag_metadata_test COMMAND tag_metadata_tests)

# CPack Packaging
# Override from CI with e.g. -DWINAMP_DISTRO_ID=ubuntu-24.04 -DCPACK_GENERATOR="DEB;TGZ"
set(CPACK_PACKAGE_NAME "winamp")
set(CPACK_PACKAGE_VENDOR "lord3nd3r")
set(CPACK_PACKAGE_CONTACT "lord3nd3r <https://github.com/lord3nd3r/winamp-linux>")
set(CPACK_PACKAGE_VERSION "${WINAMP_VERSION_FULL}")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Classic Winamp media player for Linux (${WINAMP_VERSION_FULL})")
set(CPACK_PACKAGE_HOMEPAGE_URL "https://github.com/lord3nd3r/winamp-linux")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_SOURCE_DIR}/LICENSE.md")

if(NOT DEFINED WINAMP_DISTRO_ID OR WINAMP_DISTRO_ID STREQUAL "")
    set(WINAMP_DISTRO_ID "linux")
endif()
set(CPACK_SYSTEM_NAME "${WINAMP_DISTRO_ID}")
set(CPACK_PACKAGE_FILE_NAME "winamp-${WINAMP_VERSION_FULL}-${WINAMP_DISTRO_ID}")

# Default generators; CI sets DEB / RPM / TGZ per distro family.
if(NOT CPACK_GENERATOR)
    set(CPACK_GENERATOR "DEB;TGZ")
endif()

# Debian / Ubuntu
set(CPACK_DEBIAN_PACKAGE_VERSION "${WINAMP_VERSION_FULL}")
set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
set(CPACK_DEBIAN_PACKAGE_SECTION "sound")
set(CPACK_DEBIAN_PACKAGE_PRIORITY "optional")
set(CPACK_DEBIAN_FILE_NAME "${CPACK_PACKAGE_FILE_NAME}.deb")

# Fedora / openSUSE / RHEL-family
set(CPACK_RPM_PACKAGE_LICENSE "GPL-2.0-or-later")
set(CPACK_RPM_PACKAGE_GROUP "Applications/Multimedia")
set(CPACK_RPM_PACKAGE_AUTOREQ ON)
set(CPACK_RPM_PACKAGE_AUTOPROV ON)
set(CPACK_RPM_FILE_NAME "${CPACK_PACKAGE_FILE_NAME}.rpm")

include(CPack)


