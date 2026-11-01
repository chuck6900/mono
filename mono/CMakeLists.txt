set(SUBDIRS utils io-layer metadata mini dis)

add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/../libgc/" "${CMAKE_CURRENT_SOURCE_DIR}/../libgc/")

include_directories("${CMAKE_CURRENT_SOURCE_DIR}/../libgc/include/")

set (LIBGC_LIBRARIES
        "${CMAKE_CURRENT_SOURCE_DIR}../libgc/monogc-static.lib"
)

set (GLIB2_INCLUDE_DIRS
        "${CMAKE_CURRENT_SOURCE_DIR}/../../glib/glib/include/glib-2.0/"
        "${CMAKE_CURRENT_SOURCE_DIR}/../../glib/glib/lib/glib-2.0/include"
)

set (GLIB2_LIBRARIES
        "${CMAKE_CURRENT_SOURCE_DIR}/../../glib/glib/lib/gio-2.0.lib"
        "${CMAKE_CURRENT_SOURCE_DIR}/../../glib/glib/lib/glib-2.0.lib"
        "${CMAKE_CURRENT_SOURCE_DIR}/../../glib/glib/lib/gmodule-2.0.lib"
        "${CMAKE_CURRENT_SOURCE_DIR}/../../glib/glib/lib/gobject-2.0.lib"
        "${CMAKE_CURRENT_SOURCE_DIR}/../../glib/glib/lib/gthread-2.0.lib"
)

add_compile_definitions(UNICODE)

include_directories(${GLIB2_INCLUDE_DIRS})

if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    set(AMD64 yes)
    add_compile_definitions(AMD64)
else()
#    set(X86 yes)
#    add_compile_definitions(X86)
    set(ARM yes)
    add_compile_definitions(ARM ARM_FPU_FPA)
endif()

add_compile_definitions(PLATFORM_WIN32 __WIN32__ DISABLE_JIT)

set(PLATFORM_WIN32 yes)

foreach(subdir ${SUBDIRS})
  add_subdirectory(${subdir})
endforeach()