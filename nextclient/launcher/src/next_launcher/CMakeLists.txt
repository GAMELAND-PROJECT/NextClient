set(TARGET_NAME "next_launcher")
set(OUTPUT_NAME "cstrike")

find_package(ZLIB REQUIRED)
find_package(CURL REQUIRED)
find_package(OpenGL REQUIRED)

file(GLOB_RECURSE LAUNCHER_SOURCES
        LIST_DIRECTORIES FALSE
        "*.cpp"
        "*.hpp"
        "*.h"
        "resources.rc"
        "app.manifest"
)

add_executable(${TARGET_NAME} WIN32
        ${LAUNCHER_SOURCES}
)

target_include_directories(${TARGET_NAME} PRIVATE
        ${NEXT_LAUNCHER_SOURCE_DIR}/include
)

target_link_libraries(${TARGET_NAME} PRIVATE
        nitro_api::nitro_api
        NextClient::client_mini
        NextClient::engine_mini
        NextClient::gameui
        NextClient::steam_api_proxy
        data_encoding::data_encoding
        ncl_utils::ncl_utils
        next_launcher::utils
        easyloggingpp::easyloggingpp
        TaskCoro::TaskCoro
        cpr::cpr
        CURL::libcurl
        ZLIB::ZLIB
        Dbghelp.lib
)

target_compile_definitions(${TARGET_NAME} PRIVATE
        MAX_LOGFILE_SIZE=524288 #bytes
)

set_target_properties(${TARGET_NAME} PROPERTIES
        PREFIX ""
        OUTPUT_NAME ${OUTPUT_NAME}
        SUFFIX ".exe"
        PDB_NAME ${OUTPUT_NAME}
)

#-----------------------------------------------------------------
# Export library headers
#-----------------------------------------------------------------
add_library(next_launcher_export INTERFACE)
add_library(NextClient::next_launcher ALIAS next_launcher_export)

target_include_directories(next_launcher_export INTERFACE
        ${NEXT_LAUNCHER_SOURCE_DIR}/include/next_launcher
)
