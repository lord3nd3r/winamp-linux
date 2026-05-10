cmake_minimum_required(VERSION 3.16)
project(Winamp VERSION 5.9.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

set(QT_REQUIRED_COMPONENTS Core Gui Widgets Multimedia MultimediaWidgets OpenGL)

# Prefer Qt6, but support Qt5 as a fallback.
find_package(Qt6 QUIET COMPONENTS ${QT_REQUIRED_COMPONENTS} OpenGLWidgets)
if(Qt6_FOUND)
    set(QT_PACKAGE Qt6)
    set(QT_DBUS_FOUND_VAR Qt6DBus_FOUND)
    find_package(Qt6 REQUIRED COMPONENTS ${QT_REQUIRED_COMPONENTS} OpenGLWidgets)
    find_package(Qt6 COMPONENTS DBus QUIET)
    message(STATUS "Using Qt6")
else()
    set(QT_PACKAGE Qt5)
    set(QT_DBUS_FOUND_VAR Qt5DBus_FOUND)
    find_package(Qt5 REQUIRED COMPONENTS ${QT_REQUIRED_COMPONENTS})
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

add_executable(winamp
    winamp_authentic.cpp
)

target_link_libraries(winamp
    ${QT_PACKAGE}::Core
    ${QT_PACKAGE}::Gui
    ${QT_PACKAGE}::Widgets
    ${QT_PACKAGE}::Multimedia
    ${QT_PACKAGE}::MultimediaWidgets
    ${QT_PACKAGE}::OpenGL
    ${PROJECTM_LIB}
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
install(DIRECTORY Src/Winamp/resource/ DESTINATION share/winamp/resource)
