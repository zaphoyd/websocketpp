
init_target (libwebsocketpp)

# Sources from root dir
file (GLOB SOURCE_FILES *.cpp)
file (GLOB HEADER_FILES *.hpp)

# Sources from subdirectories.
list_subdirectories (SUBDIRS ${CMAKE_CURRENT_SOURCE_DIR})
foreach (SUBDIR ${SUBDIRS})
    # Filter out certain subdirs from 3rd party code
    if (NOT ${SUBDIR} STREQUAL "sha1" AND NOT ${SUBDIR} STREQUAL "md5") # todo: AND NOT ${SUBDIR} STREQUAL "base64"
        add_source_folder (${SUBDIR})
    endif ()
endforeach ()

build_library (${TARGET_NAME} STATIC ${SOURCE_FILES} ${HEADER_FILES})

final_target ()