
init_target (libwebsocketpp)

# Sources from root dir
file (GLOB SOURCE_FILES *.cpp)
file (GLOB HEADER_FILES *.hpp)

# Sources from subdirectories.
# todo: Make this generate source groups in visual 
# studio solutions for a cleaner looking project.
list_subdirectories(SUBDIRS ${CMAKE_CURRENT_SOURCE_DIR})
foreach (SUBDIR ${SUBDIRS})
    file (GLOB SUBDIR_SOURCE_FILES ${SUBDIR}/*.cpp ${SUBDIR}/*.c)
    file (GLOB SUBDIR_HEADER_FILES ${SUBDIR}/*.hpp ${SUBDIR}/*.h)
    # Filter out certain files from 3rd party code
    if (NOT ${SUBDIR} STREQUAL "sha1" AND NOT ${SUBDIR} STREQUAL "md5")
        set (SOURCE_FILES ${SOURCE_FILES} ${SUBDIR_SOURCE_FILES})
    endif ()
    set (HEADER_FILES ${HEADER_FILES} ${SUBDIR_HEADER_FILES})
endforeach ()

# In preparation for building as dynamic lib on windows.
# This should probably be supported at some point.
# add_definitions (-DWEBSOCKETPP_MODULE_EXPORTS)

build_library (${TARGET_NAME} STATIC ${SOURCE_FILES} ${HEADER_FILES})

final_target ()