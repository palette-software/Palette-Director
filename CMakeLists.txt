PROJECT(HTTPD C)

CMAKE_MINIMUM_REQUIRED(VERSION 2.8)

SET(APACHE_INCLUDE_DIR        "/opt/apache/include"  CACHE STRING "The location of the apache include directory")

INCLUDE_DIRECTORIES( ${APACHE_INCLUDE_DIR} )


add_library(mod_palette_director MODULE
        src/mod_lbmethod_bybusyness
        )

set_target_properties(mod_palette_director PROPERTIES PREFIX "")


# Dont complain about missing symbols for our dynamically linked library
IF(APPLE)
    set_target_properties(mod_palette_director PROPERTIES LINK_FLAGS "-Wl,-flat_namespace -Wl,-undefined")
ENDIF(APPLE)


SET_TARGET_PROPERTIES(mod_palette_director PROPERTIES
        SUFFIX .so
        )
    
    
