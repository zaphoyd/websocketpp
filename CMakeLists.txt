
############ Setup project and cmake
# Minimum cmake requirement. We should require a quite recent
# cmake for the dependency find macros etc. to be up to date.
cmake_minimum_required (VERSION 3.5)
project(websocketpp VERSION 0.9.0)
############ Paths

set (WEBSOCKETPP_ROOT ${CMAKE_CURRENT_SOURCE_DIR})
set (WEBSOCKETPP_INCLUDE ${WEBSOCKETPP_ROOT}/websocketpp)
set (WEBSOCKETPP_BUILD_ROOT ${CMAKE_CURRENT_BINARY_DIR})
set (WEBSOCKETPP_ENCODING_LIBS "")
set (WEBSOCKETPP_ENCODING_DEFS "")

# Include our cmake macros
set (CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${CMAKE_CURRENT_SOURCE_DIR}/cmake)
include (CMakeHelpers)

############ Encoding/decoding support

set(WEBSOCKETPP_WITH_GZIP FALSE CACHE BOOL "If true, build support for LZ77 compression. No requirements - added to code via https://github.com/ivan-tkatchev/yalz77.")
set(WEBSOCKETPP_WITH_DEFLATE FALSE CACHE BOOL "If true, build support for deflate compression. Requires zlib.")
if (WEBSOCKETPP_WITH_GZIP OR WEBSOCKETPP_WITH_DEFLATE)
    find_package(ZLIB REQUIRED)
    if (ZLIB_FOUND)
        list(APPEND WEBSOCKETPP_ENCODING_LIBS ${ZLIB_LIBRARIES})
        list(APPEND WEBSOCKETPP_INCLUDE ${ZLIB_INCLUDE_DIRS})
        if (WEBSOCKETPP_WITH_DEFLATE)
            list(APPEND WEBSOCKETPP_ENCODING_DEFS WEBSOCKETPP_WITH_DEFLATE=1)
        endif ()
        if (WEBSOCKETPP_WITH_GZIP)
            list(APPEND WEBSOCKETPP_ENCODING_DEFS WEBSOCKETPP_WITH_GZIP=1)
        endif ()
    endif ()
endif ()

set(WEBSOCKETPP_WITH_BROTLI FALSE CACHE BOOL "If true, build support for brotli compression. Requires brotli.")
if (WEBSOCKETPP_WITH_BROTLI)
    find_package(Brotli REQUIRED)
    if (BROTLI_FOUND)
        list(APPEND WEBSOCKETPP_ENCODING_LIBS ${BROTLI_LIBRARIES})
        list(APPEND WEBSOCKETPP_INCLUDE ${BROTLI_INCLUDE_DIRS})
        list(APPEND WEBSOCKETPP_ENCODING_DEFS WEBSOCKETPP_WITH_BROTLI=1)
    endif ()
endif ()

set(WEBSOCKETPP_WITH_ZSTD FALSE CACHE BOOL "If true, build support for zstd compression. Requires zstd.")
if (WEBSOCKETPP_WITH_ZSTD)
    find_package(zstd REQUIRED)
    if (ZSTD_FOUND)
        list(APPEND WEBSOCKETPP_ENCODING_LIBS ${ZSTD_LIBRARIES})
        list(APPEND WEBSOCKETPP_INCLUDE ${ZSTD_INCLUDE_DIRS})
        list(APPEND WEBSOCKETPP_ENCODING_DEFS WEBSOCKETPP_WITH_ZSTD=1)
    endif ()
endif ()

# CMake install step prefix. I assume linux users want the prefix to
# be the default /usr or /usr/local so this is only adjusted on Windows.
# This must be set prior to any call to project or it will not be read correctly.
# - Windows: Build the INSTALL project in your solution file.
# - Linux/OSX: make install.
if (WIN32)
    set (CMAKE_INSTALL_PREFIX "${WEBSOCKETPP_ROOT}/install" CACHE PATH "")
endif ()

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

include(GNUInstallDirs)

set(INSTALL_INCLUDE_DIR include CACHE PATH "Installation directory for header files")
if (WIN32 AND NOT CYGWIN)
  set (DEF_INSTALL_CMAKE_DIR cmake)
else ()
  set (DEF_INSTALL_CMAKE_DIR ${CMAKE_INSTALL_LIBDIR}/cmake/websocketpp)
endif ()
set (INSTALL_CMAKE_DIR ${DEF_INSTALL_CMAKE_DIR} CACHE PATH "Installation directory for CMake files")

# Make relative paths absolute (needed later on)
foreach (p INCLUDE CMAKE)
  set (var INSTALL_${p}_DIR)
  if (NOT IS_ABSOLUTE "${${var}}")
    set (${var} "${CMAKE_INSTALL_PREFIX}/${${var}}")
  endif ()
endforeach ()

# Set CMake library search policy
if (COMMAND cmake_policy)
    cmake_policy (SET CMP0003 NEW)
    cmake_policy (SET CMP0005 NEW)
endif ()

# Disable unnecessary build types
set (CMAKE_CONFIGURATION_TYPES "Release;RelWithDebInfo;Debug" CACHE STRING "Configurations" FORCE)


############  Build customization

# Override from command line "CMake -D<OPTION>=TRUE/FALSE/0/1/ON/OFF"
option (ENABLE_CPP11 "Build websocketpp with CPP11 features enabled." TRUE)
option (BUILD_EXAMPLES "Build websocketpp examples." FALSE)
option (BUILD_TESTS "Build websocketpp tests." FALSE)

if (BUILD_TESTS OR BUILD_EXAMPLES)

    enable_testing ()

    ############ Compiler specific setup

    set (WEBSOCKETPP_PLATFORM_LIBS "")
    set (WEBSOCKETPP_PLATFORM_TLS_LIBS "")
    set (WEBSOCKETPP_BOOST_LIBS "")

    # VC9 and C++11 reasoning
    if (ENABLE_CPP11 AND MSVC AND MSVC90)
        message("* Detected Visual Studio 9 2008, disabling C++11 support.")
        set (ENABLE_CPP11 FALSE)
    endif ()

    # Detect clang. Not officially reported by cmake.
    execute_process(COMMAND "${CMAKE_CXX_COMPILER}" "-v" ERROR_VARIABLE CXX_VER_STDERR)
    if ("${CXX_VER_STDERR}" MATCHES ".*clang.*")
        set (CMAKE_COMPILER_IS_CLANGXX 1)
    endif ()

    # C++11 defines
    if (ENABLE_CPP11)
        if (MSVC)
            add_definitions (-D_WEBSOCKETPP_CPP11_FUNCTIONAL_)
            add_definitions (-D_WEBSOCKETPP_CPP11_SYSTEM_ERROR_)
            add_definitions (-D_WEBSOCKETPP_CPP11_RANDOM_DEVICE_)
            add_definitions (-D_WEBSOCKETPP_CPP11_MEMORY_)
        else()
            add_definitions (-D_WEBSOCKETPP_CPP11_STL_)
        endif()
    endif ()

    # Visual studio
    if (MSVC)
        set (WEBSOCKETPP_BOOST_LIBS system thread)
        set (CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /GL /Gy /GF /Ox /Ob2 /Ot /Oi /MP /arch:SSE2 /fp:fast")
        set (CMAKE_SHARED_LINKER_FLAGS_RELEASE "${CMAKE_SHARED_LINKER_FLAGS_RELEASE} /LTCG /INCREMENTAL:NO /OPT:REF /OPT:ICF")
        add_definitions (/W3 /wd4996 /wd4995 /wd4355)
        add_definitions (-DUNICODE -D_UNICODE)
        add_definitions (-D_CRT_SECURE_NO_WARNINGS -D_SCL_SECURE_NO_WARNINGS)
        add_definitions (-DNOMINMAX)
    endif ()

    # g++
    if (CMAKE_COMPILER_IS_GNUCXX)
        if (NOT APPLE AND NOT HAIKU)
            set (WEBSOCKETPP_PLATFORM_LIBS pthread rt)
        else()
        if (HAIKU)
            set (WEBSOCKETPP_PLATFORM_LIBS pthread network)
        else()
            set (WEBSOCKETPP_PLATFORM_LIBS pthread)
        endif()
        endif()
        set (WEBSOCKETPP_PLATFORM_TLS_LIBS ssl crypto)
        set (WEBSOCKETPP_BOOST_LIBS system thread)
        set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++0x")
        if (NOT APPLE)
            add_definitions (-DNDEBUG -Wall -Wcast-align) # todo: should we use CMAKE_C_FLAGS for these?
        endif ()

        # Try to detect version. Note: Not tested!
        execute_process (COMMAND ${CMAKE_CXX_COMPILER} "-dumpversion" OUTPUT_VARIABLE GCC_VERSION)
        if ("${GCC_VERSION}" STRGREATER "4.4.0")
            message("* C++11 support partially enabled due to GCC version ${GCC_VERSION}")
            set (WEBSOCKETPP_BOOST_LIBS system thread)
        endif ()
    endif ()

    # clang
    if (CMAKE_COMPILER_IS_CLANGXX)
        if (APPLE)
            set (WEBSOCKETPP_PLATFORM_LIBS pthread)
        elseif (HAIKU)
            set (WEBSOCKETPP_PLATFORM_LIBS pthread network)
        else()
            set (WEBSOCKETPP_PLATFORM_LIBS pthread rt)
        endif()
        set (WEBSOCKETPP_PLATFORM_TLS_LIBS ssl crypto)
        set (WEBSOCKETPP_BOOST_LIBS system thread)
        set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++0x")
        if (NOT APPLE)
            add_definitions (-DNDEBUG -Wall -Wno-padded) # todo: should we use CMAKE_C_FLAGS for these?
        endif ()
    endif ()

    # OSX, can override above.
    if (APPLE)
        add_definitions (-DNDEBUG -Wall)
    endif ()

    if (BUILD_EXAMPLES)
        list (APPEND WEBSOCKETPP_BOOST_LIBS random)
    endif()

    if (BUILD_TESTS)
        list (APPEND WEBSOCKETPP_BOOST_LIBS unit_test_framework)
    endif()

    ############ Dependencies

    # Set BOOST_ROOT env variable or pass with cmake -DBOOST_ROOT=path.
    # BOOST_ROOT can also be defined by a previous run from cmake cache.
    if (NOT "$ENV{BOOST_ROOT_CPP11}" STREQUAL "")
        # Scons documentation for BOOST_ROOT_CPP11:
        # "look for optional second boostroot compiled with clang's libc++ STL library
        # this prevents warnings/errors when linking code built with two different
        # incompatible STL libraries."
        file (TO_CMAKE_PATH "$ENV{BOOST_ROOT_CPP11}" BOOST_ROOT)
        set (BOOST_ROOT ${BOOST_ROOT} CACHE PATH "BOOST_ROOT dependency path" FORCE)
    endif ()
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

	if (BOOST_STATIC)
		set (Boost_USE_STATIC_LIBS TRUE)
	endif ()

    if (NOT Boost_USE_STATIC_LIBS)
        add_definitions (-DBOOST_TEST_DYN_LINK)
    endif ()

    set (Boost_FIND_REQUIRED TRUE)
    set (Boost_FIND_QUIETLY TRUE)
    set (Boost_DEBUG FALSE)
    set (Boost_USE_MULTITHREADED TRUE)
    set (Boost_ADDITIONAL_VERSIONS "1.39.0" "1.40.0" "1.41.0" "1.42.0" "1.43.0" "1.44.0" "1.46.1") # todo: someone who knows better spesify these!

    find_package (Boost 1.39.0 COMPONENTS ${WEBSOCKETPP_BOOST_LIBS})

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
        message (STATUS "-- Libraries")
        foreach (boost_lib ${Boost_LIBRARIES})
            message (STATUS "       " ${boost_lib})
        endforeach ()
        message ("")
    else ()
        message (FATAL_ERROR "Failed to find required dependency: boost")
    endif ()

    find_package(OpenSSL)
    find_package(ZLIB)
endif()

############ Add projects

# Add examples
if (BUILD_EXAMPLES)
    include_subdirs ("examples")
endif ()

# Add tests
if (BUILD_TESTS)
    include_subdirs ("test")
endif ()

print_used_build_config()

include(CMakePackageConfigHelpers)
configure_package_config_file(websocketpp-config.cmake.in
  "${PROJECT_BINARY_DIR}${CMAKE_FILES_DIRECTORY}/websocketpp-config.cmake"
  PATH_VARS INSTALL_INCLUDE_DIR
  INSTALL_DESTINATION "${INSTALL_CMAKE_DIR}"
  NO_CHECK_REQUIRED_COMPONENTS_MACRO
)
write_basic_package_version_file("${PROJECT_BINARY_DIR}${CMAKE_FILES_DIRECTORY}/websocketpp-configVersion.cmake"
  VERSION ${WEBSOCKETPP_VERSION}
  COMPATIBILITY ExactVersion)

# Install the websocketpp-config.cmake and websocketpp-configVersion.cmake
install (FILES
  "${PROJECT_BINARY_DIR}${CMAKE_FILES_DIRECTORY}/websocketpp-config.cmake"
  "${PROJECT_BINARY_DIR}${CMAKE_FILES_DIRECTORY}/websocketpp-configVersion.cmake"
  DESTINATION "${INSTALL_CMAKE_DIR}" COMPONENT dev)

add_library(websocketpp INTERFACE)
target_link_libraries(websocketpp INTERFACE ${WEBSOCKETPP_ENCODING_LIBS})
target_compile_definitions(websocketpp INTERFACE ${WEBSOCKETPP_ENCODING_DEFS})
target_include_directories(websocketpp INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})

