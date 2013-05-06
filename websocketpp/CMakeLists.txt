
init_target (libwebsocketpp)

# Sources from root dir
file (GLOB WSPP_SOURCE_FILES *.cpp)
file (GLOB WSPP_HEADER_FILES *.hpp)

# Sources from subdirectories.
list_subdirectories (SUBDIRS ${CMAKE_CURRENT_SOURCE_DIR})
foreach (SUBDIR ${SUBDIRS})
    # Filter out certain subdirs from 3rd party code
    if (NOT ${SUBDIR} STREQUAL "sha1" AND NOT ${SUBDIR} STREQUAL "md5") # todo: AND NOT ${SUBDIR} STREQUAL "base64"
        add_source_folder (${SUBDIR})
    endif ()
endforeach ()

string(TOLOWER "${WEBSOCKETPP_LIB_BUILD_TYPE}" LOWER_WEBSOCKETPP_BUILD_TYPE)
if ("${LOWER_WEBSOCKETPP_BUILD_TYPE}" STREQUAL "shared" )
    build_library (${TARGET_NAME} SHARED ${WSPP_SOURCE_FILES} ${WSPP_HEADER_FILES})
else()
    build_library (${TARGET_NAME} STATIC ${WSPP_SOURCE_FILES} ${WSPP_HEADER_FILES})
endif()

final_target ()
