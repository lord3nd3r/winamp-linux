cmake_minimum_required(VERSION 3.16)
project(Winamp VERSION 5.9.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Multimedia)

add_executable(winamp
    winamp_authentic.cpp
)

target_link_libraries(winamp
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Multimedia
)

install(TARGETS winamp RUNTIME DESTINATION bin)
install(DIRECTORY skins/ DESTINATION share/winamp/skins)
install(DIRECTORY Src/Winamp/resource/ DESTINATION share/winamp/resource)
