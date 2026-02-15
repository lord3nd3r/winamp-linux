cmake_minimum_required(VERSION 3.16)
project(winamp_qt_demo VERSION 1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Multimedia)

add_executable(winamp_demo
    winamp_authentic.cpp
)

target_link_libraries(winamp_demo
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Multimedia
)

install(TARGETS winamp_demo
    RUNTIME DESTINATION bin
)
