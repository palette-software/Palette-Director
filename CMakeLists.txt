PROJECT(HTTPD C)

CMAKE_MINIMUM_REQUIRED(VERSION 2.8)

SET(APACHE_INCLUDE_DIR        "/opt/apache/include"  CACHE STRING "The location of the apache include directory")

INCLUDE_DIRECTORIES( ${APACHE_INCLUDE_DIR} )


add_library(mod_palette_director MODULE
        src/mod_lbmethod_bybusyness.c
        src/status-pages.c src/status-pages.h src/palette-director-types.h
        src/config-loader.c src/config-loader.h

        src/csv/csv.h
        src/csv/libcsv.c
        )

set_target_properties(mod_palette_director PROPERTIES PREFIX "")


# Dont complain about missing symbols for our dynamically linked library
IF(APPLE)
    set_target_properties(mod_palette_director PROPERTIES LINK_FLAGS "-Wl,-flat_namespace -Wl,-undefined -Wl,suppress")
ENDIF(APPLE)


IF(MSVC)
  SET(APACHE_LIB_DIR        "c:/tmp/apache32/lib"  CACHE STRING "The location of the apache lib directory")

  #set_target_properties(mod_palette_director PROPERTIES LINK_FLAGS "/ignore")
  target_link_libraries(mod_palette_director
    ${APACHE_LIB_DIR}/libhttpd.lib
    ${APACHE_LIB_DIR}/libaprutil-1.lib
    ${APACHE_LIB_DIR}/libapr-1.lib)

ENDIF(MSVC)


SET_TARGET_PROPERTIES(mod_palette_director PROPERTIES
        SUFFIX .so
        )
