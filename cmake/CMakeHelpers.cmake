
# Print build configuration
macro (print_used_build_config)
    message ("\n=========== Used Build Configuration =============\n")
    message (STATUS "ENABLE_CPP11        = " ${ENABLE_CPP11})
    message (STATUS "BUILD_EXAMPLES      = " ${BUILD_EXAMPLES})
    message (STATUS "BUILD_TESTS         = " ${BUILD_TESTS})
    message ("")
    message (STATUS "WEBSOCKETPP_ROOT    = " ${WEBSOCKETPP_ROOT})
    message (STATUS "WEBSOCKETPP_BIN     = " ${WEBSOCKETPP_BIN})
    message (STATUS "WEBSOCKETPP_LIB     = " ${WEBSOCKETPP_LIB})
    message (STATUS "Install prefix      = " ${CMAKE_INSTALL_PREFIX})
    message ("")
    message (STATUS "WEBSOCKETPP_BOOST_LIBS        = ${WEBSOCKETPP_BOOST_LIBS}")
    message (STATUS "WEBSOCKETPP_PLATFORM_LIBS     = ${WEBSOCKETPP_PLATFORM_LIBS}")
    message (STATUS "WEBSOCKETPP_PLATFORM_TSL_LIBS = ${WEBSOCKETPP_PLATFORM_TSL_LIBS}")
    message ("") 
endmacro ()

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

# Adds the given folder_name into the source files of the current project. 
# Use this macro when your module contains .cpp and .h files in several subdirectories.
# Your sources variable needs to be WSPP_SOURCE_FILES and headers variable WSPP_HEADER_FILES.
macro(add_source_folder folder_name)
    file(GLOB H_FILES_IN_FOLDER_${folder_name} ${folder_name}/*.hpp ${folder_name}/*.h)
    file(GLOB CPP_FILES_IN_FOLDER_${folder_name} ${folder_name}/*.cpp ${folder_name}/*.c)
    source_group("Header Files\\${folder_name}" FILES ${H_FILES_IN_FOLDER_${folder_name}})
    source_group("Source Files\\${folder_name}" FILES ${CPP_FILES_IN_FOLDER_${folder_name}})
    set(WSPP_HEADER_FILES ${WSPP_HEADER_FILES} ${H_FILES_IN_FOLDER_${folder_name}})
    set(WSPP_SOURCE_FILES ${WSPP_SOURCE_FILES} ${CPP_FILES_IN_FOLDER_${folder_name}})
endmacro()

# Initialize target.
macro (init_target NAME)
    set (TARGET_NAME ${NAME})
    message ("** " ${TARGET_NAME})
    
    # Include our own module path. This makes #include "x.h" 
    # work in project subfolders to include the main directory headers.
    include_directories (${CMAKE_CURRENT_SOURCE_DIR})
endmacro ()

# Build library for static and shared libraries
macro (build_library TARGET_NAME LIB_TYPE)
    set (TARGET_LIB_TYPE ${LIB_TYPE})
    message (STATUS "-- Build Type:")
    message (STATUS "       " ${TARGET_LIB_TYPE} " library")

    add_library (${TARGET_NAME} ${TARGET_LIB_TYPE} ${ARGN})

    target_link_libraries (${TARGET_NAME} ${WEBSOCKETPP_PLATFORM_LIBS})

    if (MSVC)
        if (${TARGET_LIB_TYPE} STREQUAL "SHARED")
            set_target_properties (${TARGET_NAME} PROPERTIES LINK_FLAGS_RELEASE ${CMAKE_SHARED_LINKER_FLAGS_RELEASE})
        else ()
            set_target_properties (${TARGET_NAME} PROPERTIES STATIC_LIBRARY_FLAGS_RELEASE "/LTCG")
        endif ()
    endif ()

    message (STATUS "-- Build Destination:")
    message (STATUS "       " ${WEBSOCKETPP_LIB})

    set_target_properties (${TARGET_NAME} PROPERTIES ARCHIVE_OUTPUT_DIRECTORY ${WEBSOCKETPP_LIB})
    set_target_properties (${TARGET_NAME} PROPERTIES DEBUG_POSTFIX d)
endmacro ()

# Build executable for executables
macro (build_executable TARGET_NAME)
    set (TARGET_LIB_TYPE "EXECUTABLE")
    message (STATUS "-- Build Type:")
    message (STATUS "       " ${TARGET_LIB_TYPE})

    add_executable (${TARGET_NAME} ${ARGN})

    include_directories (${WEBSOCKETPP_ROOT} ${WEBSOCKETPP_INCLUDE})

    set_target_properties (${TARGET_NAME} PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${WEBSOCKETPP_BIN})
    set_target_properties (${TARGET_NAME} PROPERTIES DEBUG_POSTFIX d)
endmacro ()

# Finalize target for all types
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
endmacro ()

macro (link_boost)
    target_link_libraries (${TARGET_NAME} ${Boost_LIBRARIES})
endmacro ()
