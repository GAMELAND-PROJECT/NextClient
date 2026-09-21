cmake_minimum_required(VERSION 3.21)
project(updater VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)

add_executable(${PROJECT_NAME} WIN32 main.cpp)

target_link_libraries(${PROJECT_NAME} PRIVATE
    urlmon.lib
    shell32.lib
    user32.lib
    shlwapi.lib
)

if (WIN32)
    target_compile_definitions(${PROJECT_NAME} PRIVATE
            _WIN32
            WIN32
            _UNICODE
            UNICODE
            WIN32_LEAN_AND_MEAN
    )
endif()

set_target_properties(${PROJECT_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/$<CONFIG>"
)
