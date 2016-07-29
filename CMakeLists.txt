PROJECT(HTTPD C)

CMAKE_MINIMUM_REQUIRED(VERSION 2.8)

SET(LIBHTTPD_LIB "c:/tmp/apache/lib/libhttpd.lib")
SET(APR_LIB "c:/tmp/apache/lib/apr-1.lib")
SET(APRUTIL_LIB "c:/tmp/apache/lib/aprutil-1.lib")

INCLUDE_DIRECTORIES(
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/libs/apr/include
    ${CMAKE_CURRENT_SOURCE_DIR}/libs/apr-util/include
    ${CMAKE_CURRENT_SOURCE_DIR}/libs/libhttpd/include
    
    ${CMAKE_CURRENT_SOURCE_DIR}/libs/os/win32
)


add_library( mod_palette_director SHARED
    src/mod_lbmethod_bybusyness
    
    ${CMAKE_CURRENT_SOURCE_DIR}/libs/libhttpd/win32/httpd.rc
 )
 
target_link_libraries( mod_palette_director
    ${LIBHTTPD_LIB} ${APR_LIB} ${APRUTIL_LIB} "c:/tmp/apache/lib/libaprutil-1.lib"
    )
    
SET_TARGET_PROPERTIES(mod_palette_director PROPERTIES
      SUFFIX .so
      LINK_FLAGS /base:@${CMAKE_CURRENT_SOURCE_DIR}/libs/libhttpd/BaseAddr.ref,mod_palette_director.so
    )
    
    
