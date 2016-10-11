PROJECT(PALETTE_DIRECTOR C)

CMAKE_MINIMUM_REQUIRED(VERSION 2.8)

LIST(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

find_package(APACHE REQUIRED)
# find_package(APR REQUIRED)
include_directories(${APACHE_INCLUDE_DIR})

SET(APACHE_LIB_DIR "${APACHE_INCLUDE_DIR}/../lib")       

add_library(mod_palette_director MODULE
        src/palette-macros.h

        src/mod_lbmethod_bybusyness.c
        src/status-pages.c src/status-pages.h

        src/palette-director-types.h
        src/palette-director-types.c

        src/config-loader.c
        src/config-loader.h

        src/csv/csv.h
        src/csv/libcsv.c

        src/win/inet_ntop.h
        src/win/inet_ntop.c
        )

set_target_properties(mod_palette_director PROPERTIES PREFIX "")


set(INSTALLER_COMPONENTS_PATH "${CMAKE_CURRENT_SOURCE_DIR}/nsis")
FILE(TO_NATIVE_PATH ${INSTALLER_COMPONENTS_PATH} INSTALLER_COMPONENTS_PATH )
message("CURRENT: ${INSTALLER_COMPONENTS_PATH}")

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/cmake/CPackOptions.cmake.in CPackOptions.cmake @ONLY)
set(CPACK_PROJECT_CONFIG_FILE "${CMAKE_BINARY_DIR}/CPackOptions.cmake")


# Dont complain about missing symbols for our dynamically linked library
IF(APPLE)
    set_target_properties(mod_palette_director PROPERTIES LINK_FLAGS "-Wl,-flat_namespace -Wl,-undefined -Wl,suppress")
ENDIF(APPLE)


IF(MSVC)
  message("APACHE_LIB_DIR: ${APACHE_LIB_DIR}")

  target_link_libraries(mod_palette_director
    ${APACHE_LIB_DIR}/libhttpd.lib
    ${APACHE_LIB_DIR}/libaprutil-1.lib
    ${APACHE_LIB_DIR}/libapr-1.lib
          wsock32 ws2_32
          )

ENDIF(MSVC)

# Install
# =============================================================


SET_TARGET_PROPERTIES(mod_palette_director PROPERTIES
        SUFFIX .so
        )


install(TARGETS mod_palette_director
   ARCHIVE
   LIBRARY DESTINATION modules
   COMPONENT libraries)

 install(FILES
  doc/installer/INSTALL.md
  doc/installer/HOWTO.md
  DESTINATION docs
  )

install(FILES
  doc/config/httpd.mod-palette-director.conf
  doc/config/workers.csv
  DESTINATION config
  )

set(CPACK_GENERATOR NSIS)
set(CPACK_PACKAGE_NAME "mod_palette_director")
set(CPACK_PACKAGE_VENDOR "Palette Software")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Apache Module for Palette Director")
set(CPACK_PACKAGE_VERSION "1.0.0")
set(CPACK_PACKAGE_VERSION_MAJOR "1")
set(CPACK_PACKAGE_VERSION_MINOR "0")
set(CPACK_PACKAGE_VERSION_PATCH "0")
set(CPACK_PACKAGE_INSTALL_DIRECTORY "Palette Director")

SET(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/doc/installer/LICENSE.txt")
SET(CPACK_RESOURCE_FILE_README "${CMAKE_CURRENT_SOURCE_DIR}/doc/installer/README.txt")
SET(CPACK_RESOURCE_FILE_WELCOME "${CMAKE_CURRENT_SOURCE_DIR}/doc/installer/WELCOME.txt")
#SET(CPACK_NSIS_MODIFY_PATH ON)

INCLUDE(CPack)
