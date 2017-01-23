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
	set (WEBSOCKETPP_PLATFORM_LIBS pthread rt)
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
if (NOT APPLE)
	set (WEBSOCKETPP_PLATFORM_LIBS pthread rt)
else()
	set (WEBSOCKETPP_PLATFORM_LIBS pthread)
endif()
	set (WEBSOCKETPP_PLATFORM_TLS_LIBS ssl crypto)
	set (WEBSOCKETPP_BOOST_LIBS system thread)
	set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++0x -stdlib=libc++") # todo: is libc++ really needed here?
if (NOT APPLE)
	add_definitions (-DNDEBUG -Wall -Wno-padded) # todo: should we use CMAKE_C_FLAGS for these?
endif ()
endif ()

# OSX, can override above.
if (APPLE)
	add_definitions (-DNDEBUG -Wall)
endif ()

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
	add_definitions (/DBOOST_TEST_DYN_LINK)
endif ()

set (Boost_FIND_REQUIRED TRUE)
set (Boost_FIND_QUIETLY TRUE)
set (Boost_DEBUG FALSE)
set (Boost_USE_MULTITHREADED TRUE)
set (Boost_ADDITIONAL_VERSIONS "1.39.0" "1.40.0" "1.41.0" "1.42.0" "1.43.0" "1.44.0" "1.46.1") # todo: someone who knows better spesify these!

find_package (Boost 1.39.0 COMPONENTS "${WEBSOCKETPP_BOOST_LIBS}")

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