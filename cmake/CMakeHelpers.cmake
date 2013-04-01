
# Lists all subdirectories to the RESULT variable.
macro (list_subdirectories RESULT curdir)
    set (_TEMP_DIRLIST "")
    file (GLOB children RELATIVE ${curdir} ${curdir}/*)
    foreach (child ${children})
        if (IS_DIRECTORY ${curdir}/${child})
            set (_TEMP_DIRLIST ${_TEMP_DIRLIST} ${child})
        endif ()
    endforeach ()
    set (${RESULT} ${_TEMP_DIRLIST})
endmacro ()

# Initialize target.
macro (init_target NAME)
    set (TARGET_NAME ${NAME})   
    message ("** " ${TARGET_NAME})
    
    # Include our own module path. This makes #include "x.h" 
    # work in project subfolders to include the main directory headers.
    include_directories (${CMAKE_CURRENT_SOURCE_DIR})
endmacro ()

macro (build_library TARGET_NAME LIB_TYPE)
    # STATIC or SHARED
    set (TARGET_LIB_TYPE ${LIB_TYPE})

    message (STATUS "-- Build Type:")
    message (STATUS "       " ${TARGET_LIB_TYPE} " library")
   
    add_library (${TARGET_NAME} ${TARGET_LIB_TYPE} ${ARGN})

    if (MSVC)
        if (${TARGET_LIB_TYPE} STREQUAL "SHARED")
            set_target_properties (${TARGET_NAME} PROPERTIES LINK_FLAGS_RELEASE "${CMAKE_SHARED_LINKER_FLAGS_RELEASE} /LTCG /INCREMENTAL:NO /OPT:REF /OPT:ICF")
        else ()
            set_target_properties (${TARGET_NAME} PROPERTIES STATIC_LIBRARY_FLAGS_RELEASE "/LTCG")
        endif ()
    endif ()

    if (${TARGET_LIB_TYPE} STREQUAL "STATIC")
        message (STATUS "-- Build Destination:")
        message (STATUS "       " ${WEBSOCKETPP_LIB})
        set_target_properties (${TARGET_NAME} PROPERTIES ARCHIVE_OUTPUT_DIRECTORY ${WEBSOCKETPP_LIB})
    endif ()

    set_target_properties (${TARGET_NAME} PROPERTIES DEBUG_POSTFIX d)
endmacro (build_library)

macro (build_executable TARGET_NAME)
    set (TARGET_LIB_TYPE "EXECUTABLE")
    add_executable (${TARGET_NAME} ${ARGN})
    set_target_properties (${TARGET_NAME} PROPERTIES DEBUG_POSTFIX d)
endmacro (build_executable)

macro (final_target)
    if (${TARGET_LIB_TYPE} STREQUAL "SHARED")
        install (TARGETS ${TARGET_NAME} 
                 LIBRARY DESTINATION "bin" # non DLL platforms shared libs are LIBRARY
                 RUNTIME DESTINATION "bin" # DLL platforms shared libs are RUNTIME
                 ARCHIVE DESTINATION "lib" # DLL platforms static link part of the shared lib are ARCHIVE
                 CONFIGURATIONS ${CMAKE_CONFIGURATION_TYPES})
    endif ()
    if (${TARGET_LIB_TYPE} STREQUAL "STATIC")
        install (TARGETS ${TARGET_NAME} 
                 ARCHIVE DESTINATION "lib" 
                 CONFIGURATIONS ${CMAKE_CONFIGURATION_TYPES})
    endif ()
    if (${TARGET_LIB_TYPE} STREQUAL "EXECUTABLE")
        install (TARGETS ${TARGET_NAME} 
                 RUNTIME DESTINATION "bin" 
                 CONFIGURATIONS ${CMAKE_CONFIGURATION_TYPES})
    endif ()
    
    # install headers, directly from current source dir and look for subfolders with headers
    file (GLOB TARGET_INSTALL_HEADERS *.hpp)
    install (FILES ${TARGET_INSTALL_HEADERS} DESTINATION "include/${TARGET_NAME}")
    list_subdirectories(SUBDIRS ${CMAKE_CURRENT_SOURCE_DIR})
    foreach (SUBDIR ${SUBDIRS})
        file (GLOB TARGET_INSTALL_HEADERS_SUBDIR ${SUBDIR}/*.hpp)
        install (FILES ${TARGET_INSTALL_HEADERS_SUBDIR} DESTINATION "include/${TARGET_NAME}/${SUBDIR}")
    endforeach ()

    # Pretty printing
    message ("")
endmacro (final_target)