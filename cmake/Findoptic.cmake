if (TARGET optic::optic)
    return()
endif()

include(FindPackageHandleStandardArgs)

###
### Find includes and libraries
###

find_path(OPTIC_INCLUDE_DIR
        NAMES optick.h
        PATHS ${NEXTCLIENT_ROOT}/dep/optic/include
        NO_DEFAULT_PATH
        NO_CACHE
)

if (USE_PROFILER)
    find_library(OPTIC_LIBRARY
            NAMES OptickCore.lib
            PATHS ${NEXTCLIENT_ROOT}/dep/optic/lib
            NO_DEFAULT_PATH
            NO_CACHE
    )
    find_package_handle_standard_args(optic REQUIRED_VARS OPTIC_LIBRARY OPTIC_INCLUDE_DIR)
else ()
    find_package_handle_standard_args(optic REQUIRED_VARS OPTIC_INCLUDE_DIR)
endif ()

###
### Setup library
###

if (USE_PROFILER)
    add_library(optic::optic STATIC IMPORTED GLOBAL)
    set_target_properties(optic::optic PROPERTIES
            IMPORTED_LOCATION "${OPTIC_LIBRARY}"
    )
else ()
    # Sources still include optick.h, but disabled Release builds must not carry
    # an OptickCore link/import or runtime DLL dependency.
    add_library(optic::optic INTERFACE IMPORTED GLOBAL)
endif ()

target_include_directories(optic::optic INTERFACE
        ${OPTIC_INCLUDE_DIR}
)
