cmake_minimum_required(VERSION 3.21)
project(speex)

if (MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif ()

file(GLOB SPEEX_SOURCES "src/*.c")

add_library(speex STATIC
    ${SPEEX_SOURCES}
)

add_library(speex::speex ALIAS speex)

target_include_directories(speex PUBLIC
    include
)

target_include_directories(speex PRIVATE
    src
)

target_compile_definitions(speex PUBLIC
    FLOATING_POINT
    EXPORT=
)

target_compile_definitions(speex PRIVATE
    _CRT_SECURE_NO_WARNINGS
    HAVE_CONFIG_H
)

if (MSVC)
    target_compile_options(speex PRIVATE
        /wd4244
        /wd4305
        /wd4101
        /wd4996
    )
endif ()
