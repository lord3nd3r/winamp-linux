# Nu library - Nullsoft Utility library (header-only for now)
project(nu)

# Header-only library for utilities
add_library(nu INTERFACE)

target_include_directories(nu INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}
)
