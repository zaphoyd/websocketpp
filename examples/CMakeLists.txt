
# Roams all subdirs for CMakeLists.txt
list_subdirectories(SUBDIRS ${CMAKE_CURRENT_SOURCE_DIR})
foreach (SUBDIR ${SUBDIRS})
    if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${SUBDIR}/CMakeLists.txt")
        add_subdirectory (${SUBDIR})
    endif ()
endforeach ()
