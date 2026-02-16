# Winamp main application
project(winamp)

set(WINAMP_SOURCES
    # Qt6 Linux entry point (replaces WinMain)
    linux/main_linux.cpp
    
    # Window management (Qt6 adapted)
    linux/main_window_qt.cpp
    
    # Minimal stubs for now
    rand.cpp
)

set(WINAMP_HEADERS
    Main.h
    api_winamp.h
    application.h
)

# Qt6-specific UI files
set(WINAMP_UI_SOURCES
    linux/ui/MainWindowQt.ui
)

add_executable(winamp
    ${WINAMP_SOURCES}
    ${WINAMP_HEADERS}
)

target_link_libraries(winamp
    wasabi
    nu
    tataki
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Multimedia
    Qt6::MultimediaWidgets
    Qt6::OpenGL
    Qt6::Network
    ZLIB::ZLIB
)

target_include_directories(winamp PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/linux
)

target_compile_definitions(winamp PRIVATE
    LINUX
    QT_NO_DEBUG_OUTPUT
)
