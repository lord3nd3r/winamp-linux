cmake_minimum_required(VERSION 3.16)
project(Winamp VERSION 0.5.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Multimedia MultimediaWidgets OpenGL OpenGLWidgets)

# Qt6 DBus for MPRIS2 media player desktop integration (optional)
find_package(Qt6 COMPONENTS DBus QUIET)
if(Qt6DBus_FOUND)
    message(STATUS "Qt6 DBus found — MPRIS2 media player integration enabled")
else()
    message(STATUS "Qt6 DBus not found — MPRIS2 disabled (media keys won't work)")
endif()

# projectM — Milkdrop-compatible visualization engine
# Install: sudo apt-get install libprojectm-dev projectm-data
find_library(PROJECTM_LIB projectM REQUIRED)

add_executable(winamp
    src/main.cpp
    src/eq10dsp.cpp
    src/skinutils.cpp
    src/translator.cpp
    src/bookmarkmanager.cpp
    src/recentfilesmanager.cpp
    src/winampbitmaps.cpp
    src/eqpresets.cpp
    src/modernskinengine.cpp
    src/dialogs.cpp
    src/videowindow.cpp
    src/milkdropwindow.cpp
    src/mpris2.cpp
    src/medialibrarywindow.cpp
    src/playlistwindow.cpp
    src/equalizerwindow.cpp
    src/winampwindow.cpp
)

target_include_directories(winamp PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

target_link_libraries(winamp
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Multimedia
    Qt6::MultimediaWidgets
    Qt6::OpenGL
    Qt6::OpenGLWidgets
    ${PROJECTM_LIB}
)

# Conditionally link Qt6 DBus if available
if(Qt6DBus_FOUND)
    target_link_libraries(winamp Qt6::DBus)
    target_compile_definitions(winamp PRIVATE QT_DBUS_LIB)
endif()

install(TARGETS winamp RUNTIME DESTINATION bin)
install(DIRECTORY skins/ DESTINATION share/winamp/skins)
install(DIRECTORY lang/ DESTINATION share/winamp/lang)
install(DIRECTORY Src/Winamp/resource/ DESTINATION share/winamp/resource)

# Desktop integration files
install(FILES packaging/winamp.desktop DESTINATION share/applications)
install(FILES packaging/org.winamp.Winamp.metainfo.xml DESTINATION share/metainfo)

# Install icon at multiple standard sizes (using the .ico from resources)
# For now install a single scalable reference; distro packagers can add PNG sizes
install(FILES Src/Winamp/resource/WinampIcon.ico DESTINATION share/icons/hicolor/256x256/apps RENAME winamp.ico)

# ──────────────────────────────────────────────
# CPack — generate .deb, .rpm, and .tar.gz
# ──────────────────────────────────────────────
set(CPACK_PACKAGE_NAME "winamp")
set(CPACK_PACKAGE_VERSION "0.5.0")
set(CPACK_PACKAGE_RELEASE "beta1")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Winamp media player for Linux (Qt6 port)")
set(CPACK_PACKAGE_DESCRIPTION "A native Linux port of the legendary Winamp media player using Qt6. Features the classic Winamp look and feel with support for classic skins, equalizer, playlist editor, media library, and MilkDrop visualizations via projectM.")
set(CPACK_PACKAGE_VENDOR "Nullsoft")
set(CPACK_PACKAGE_CONTACT "lord3nd3r@github.com")
set(CPACK_PACKAGE_HOMEPAGE_URL "https://github.com/lord3nd3r/winamp-linux")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE.md")
set(CPACK_PACKAGE_FILE_NAME "${CPACK_PACKAGE_NAME}-${CPACK_PACKAGE_VERSION}-${CPACK_PACKAGE_RELEASE}-${CMAKE_SYSTEM_PROCESSOR}")

# DEB-specific
set(CPACK_DEBIAN_PACKAGE_MAINTAINER "lord3nd3r <lord3nd3r@github.com>")
set(CPACK_DEBIAN_PACKAGE_SECTION "sound")
set(CPACK_DEBIAN_PACKAGE_PRIORITY "optional")
set(CPACK_DEBIAN_PACKAGE_DEPENDS "libqt6core6 (>= 6.2), libqt6gui6 (>= 6.2), libqt6widgets6 (>= 6.2), libqt6multimedia6 (>= 6.2), libqt6opengl6 (>= 6.2), libprojectm-dev, libgl1-mesa-glx | libgl1")
set(CPACK_DEBIAN_PACKAGE_HOMEPAGE "https://github.com/lord3nd3r/winamp-linux")

# RPM-specific
set(CPACK_RPM_PACKAGE_LICENSE "GPLv2")
set(CPACK_RPM_PACKAGE_GROUP "Applications/Multimedia")
set(CPACK_RPM_PACKAGE_REQUIRES "qt6-qtbase >= 6.2, qt6-qtmultimedia >= 6.2, mesa-libGL, libprojectM")
set(CPACK_RPM_PACKAGE_URL "https://github.com/lord3nd3r/winamp-linux")

# Generator list
set(CPACK_GENERATOR "TGZ")

include(CPack)
