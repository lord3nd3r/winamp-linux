cmake_minimum_required(VERSION 3.16)
project(Winamp VERSION 5.9.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Multimedia OpenGL OpenGLWidgets)

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
    winamp_authentic.cpp
)

target_link_libraries(winamp
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Multimedia
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
install(DIRECTORY Src/Winamp/resource/ DESTINATION share/winamp/resource)
