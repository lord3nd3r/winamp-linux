# Tataki - Audio decoding library
project(tataki)

add_library(tataki STATIC
    main.cpp
)

target_include_directories(tataki PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(tataki
    wasabi
)
