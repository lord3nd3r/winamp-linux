# Wasabi Framework - Cross-platform UI library
project(wasabi)

set(WASABI_SOURCES
    # Platform abstraction
    bfc/platform/platform.cpp
    bfc/platform/linux/linux.cpp
    
    # Core BFC (Base Foundation Classes)
    bfc/string/string.cpp
    bfc/memblock.cpp
    bfc/ptrlist.cpp
    bfc/wasabi_std.cpp
    bfc/thread.cpp
    bfc/critsec.cpp
    bfc/nsguid.cpp
    bfc/std_file.cpp
    bfc/std_string.cpp
    
    # Window API - Qt implementation
    api/wnd/wndapi.cpp
    api/wnd/basewnd.cpp
)

set(WASABI_HEADERS
    bfc/platform/platform.h
    bfc/platform/linux.h
    bfc/platform/types.h
    api/wnd/api_wnd.h
    api/wnd/basewnd.h
    api/service/api_service.h
)

# Qt6 wrapper layer - bridges Win32 to Qt6  
# Built separately to avoid namespace pollution
set(WASABI_QT_SOURCES
    qt6/QtWindowAdapter.cpp
    qt6/QtCanvasAdapter.cpp
)

add_library(wasabi_qt SHARED
    ${WASABI_QT_SOURCES}
)

target_link_libraries(wasabi_qt
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
)

# Tell wasabi_qt where to find basic types without pulling in all Wasabi
target_include_directories(wasabi_qt PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/bfc/platform
)

add_library(wasabi SHARED
    ${WASABI_SOURCES}
)

target_link_libraries(wasabi
    wasabi_qt
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    pthread
    dl
    X11
)

target_include_directories(wasabi PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/api
    ${CMAKE_CURRENT_SOURCE_DIR}/bfc
)

target_compile_definitions(wasabi PRIVATE
    WASABI_COMPILE_WND
    WASABI_COMPILE_FONTS
    WASABI_API_EXPORTS
)
