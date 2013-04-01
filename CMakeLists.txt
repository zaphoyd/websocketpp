
# Project name
project (websocketpp)

# Minimum cmake requirement. We should require a quite recent
# cmake for the dependency find macros etc. to be up to date.
cmake_minimum_required (VERSION 2.5)

# Include our cmake macros
set (CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} cmake)
include (CMakeHelpers)

# Build customization: 
# - Override from command line "CMake -D<OPTION>=TRUE/FALSE/0/1". Remove CMakeCache.txt before the run.
option (ENABLE_CPP11 "Build websocketpp with CPP11 features enabled." FALSE) # todo
option (BUILD_EXAMPLES "Build websocketpp examples." FALSE)
option (BUILD_TESTS "Build websocketpp tests." FALSE) # todo

# Disable unnecessary build types
set (CMAKE_CONFIGURATION_TYPES "Release;RelWithDebInfo;Debug" CACHE STRING "Configurations" FORCE)

# Spesify useful paths
set (WEBSOCKETPP_ROOT ${CMAKE_CURRENT_SOURCE_DIR})
set (WEBSOCKETPP_BIN ${WEBSOCKETPP_ROOT}/bin)
set (WEBSOCKETPP_LIB ${WEBSOCKETPP_ROOT}/lib)
set (WEBSOCKETPP_INCLUDE ${WEBSOCKETPP_ROOT}/websocketpp)
set (WEBSOCKETPP_PREFIX ${WEBSOCKETPP_ROOT}/build)

# CMake install step prefix. I assume linux users want the prefix to 
# be the default usr so this is only adjusted on Windows.
# How to invoke:
# - Windows: Build the INSTALL project in your solution file.
# - Linux/OSX: make install.
if (MSVC)
	set (CMAKE_INSTALL_PREFIX ${WEBSOCKETPP_PREFIX})
endif ()

# Find and configure Boost dependency. You should define BOOST_ROOT environment variable or
# pass it to cmake with -DBOOST_ROOT=path. CMake variable BOOST_ROOT can be defined at this stage by -D or from cache.
if ("${BOOST_ROOT}" STREQUAL "")
    file (TO_CMAKE_PATH "$ENV{BOOST_ROOT}" BOOST_ROOT)
    # Cache BOOST_ROOT for runs that do not define $ENV{BOOST_ROOT}.
    set (BOOST_ROOT ${BOOST_ROOT} CACHE PATH "BOOST_ROOT dependency path" FORCE)
endif ()

message ("* Configuring Boost")
message (STATUS "-- Using BOOST_ROOT")
message (STATUS "       " ${BOOST_ROOT})

if (MSVC)
    set (Boost_USE_MULTITHREADED TRUE)
    set (Boost_USE_STATIC_LIBS TRUE)
else ()
    set (Boost_USE_MULTITHREADED FALSE)
    set (Boost_USE_STATIC_LIBS FALSE)
endif ()

set (Boost_FIND_REQUIRED TRUE)
set (Boost_FIND_QUIETLY TRUE)
set (Boost_DEBUG FALSE)
set (Boost_USE_MULTITHREADED TRUE)
set (Boost_ADDITIONAL_VERSIONS "1.39.0" "1.40.0" "1.41.0" "1.42.0" "1.43.0" "1.44.0" "1.46.1") # todo: someone who knows better spesify these!

find_package (Boost 1.39.0 COMPONENTS system thread regex) # todo: What do we need for each platform?

if (Boost_FOUND)
    # Boost is a project wide global dependency.
    include_directories (${Boost_INCLUDE_DIRS})
    link_directories (${Boost_LIBRARY_DIRS})

    # Pretty print status
    message (STATUS "-- Include Directories")
    foreach (include_dir ${Boost_INCLUDE_DIRS})
        message (STATUS "       " ${include_dir})
    endforeach ()
    message (STATUS "-- Library Directories")
    foreach (library_dir ${Boost_LIBRARY_DIRS})
        message (STATUS "       " ${library_dir})
    endforeach ()
    message ("")
else ()
    message (FATAL_ERROR "Failed to find required dependency: boost")
endif ()

# Add main library
add_subdirectory (websocketpp)

# Add examples
if (BUILD_EXAMPLES)
    add_subdirectory (examples)
endif ()

# Add tests
if (BUILD_TESTS)
    add_subdirectory (test)
endif ()
