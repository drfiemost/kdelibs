# - Try to find the KExiv2 library
#
# If you have put a local version of libkexiv2 into your source tree,
# set KEXIV2_LOCAL_DIR to the relative path to the local directory.
#
# Once done this will define
#
#  KEXIV2_FOUND - system has libkexiv2
#  KEXIV2_INCLUDE_DIR - the libkexiv2 include directory
#  KEXIV2_LIBRARIES - Link these to use libkexiv2
#  KEXIV2_DEFINITIONS - Compiler switches required for using libkexiv2
#

# Copyright (c) 2008-2014, Gilles Caulier, <caulier.gilles@gmail.com>
#
# Redistribution and use is allowed according to the terms of the BSD license.
# For details see the accompanying COPYING-CMAKE-SCRIPTS file.

if (KEXIV2_INCLUDE_DIR AND KEXIV2_LIBRARIES AND KEXIV2_DEFINITIONS)

  message(STATUS "Found Kexiv2 library in cache: ${KEXIV2_LIBRARIES}")

  # in cache already
  set(KEXIV2_FOUND TRUE)

else (KEXIV2_INCLUDE_DIR AND KEXIV2_LIBRARIES AND KEXIV2_DEFINITIONS)

  message(STATUS "Check Kexiv2 library in local sub-folder...")

  # Check if library is not in local sub-folder

  if (KEXIV2_LOCAL_DIR)
    set(KEXIV2_LOCAL_FOUND TRUE)
  else (KEXIV2_LOCAL_DIR)
    find_file(KEXIV2_LOCAL_FOUND libkexiv2/libkexiv2_export.h ${CMAKE_SOURCE_DIR}/libkexiv2 ${CMAKE_SOURCE_DIR}/libs/libkexiv2 NO_DEFAULT_PATH)

    if (KEXIV2_LOCAL_FOUND)
      # Was it found in libkexiv2/ or in libs/libkexiv2?
      find_file(KEXIV2_LOCAL_FOUND_IN_LIBS libkexiv2/libkexiv2_export.h ${CMAKE_SOURCE_DIR}/libs/libkexiv2 NO_DEFAULT_PATH)
      if (KEXIV2_LOCAL_FOUND_IN_LIBS)
        set(KEXIV2_LOCAL_DIR libs/libkexiv2)
      else (KEXIV2_LOCAL_FOUND_IN_LIBS)
        set(KEXIV2_LOCAL_DIR libkexiv2)
      endif (KEXIV2_LOCAL_FOUND_IN_LIBS)
    endif (KEXIV2_LOCAL_FOUND)

  endif (KEXIV2_LOCAL_DIR)

  if (KEXIV2_LOCAL_FOUND)
    # we need two include directories: because the version.h file is put into the build directory
    # TODO KEXIV2_INCLUDE_DIR sounds like it should contain only one directory...
    set(KEXIV2_INCLUDE_DIR ${CMAKE_SOURCE_DIR}/${KEXIV2_LOCAL_DIR} ${CMAKE_BINARY_DIR}/${KEXIV2_LOCAL_DIR})
    set(KEXIV2_DEFINITIONS "-I${CMAKE_SOURCE_DIR}/${KEXIV2_LOCAL_DIR}" "-I${CMAKE_BINARY_DIR}/${KEXIV2_LOCAL_DIR}")
    set(KEXIV2_LIBRARIES kexiv2)
    message(STATUS "Found Kexiv2 library in local sub-folder: ${CMAKE_SOURCE_DIR}/${KEXIV2_LOCAL_DIR}")
    set(KEXIV2_FOUND TRUE)
    mark_as_advanced(KEXIV2_INCLUDE_DIR KEXIV2_LIBRARIES KEXIV2_DEFINITIONS)

  else(KEXIV2_LOCAL_FOUND)
    if(NOT WIN32) 
      message(STATUS "Check Kexiv2 library using pkg-config...")

      # use pkg-config to get the directories and then use these values
      # in the FIND_PATH() and FIND_LIBRARY() calls
      find_package(PkgConfig QUIET)

      if(PKG_CONFIG_FOUND)
        pkg_check_modules(_pc_KEXIV2 libkexiv2>=0.2.0)
      endif(PKG_CONFIG_FOUND)
    
    else(NOT WIN32)
      set(_pc_KEXIV2_FOUND TRUE)
    endif(NOT WIN32)

    if(_pc_KEXIV2_FOUND)
        find_path(KEXIV2_INCLUDE_DIR libkexiv2/version.h
          ${_pc_KEXIV2_INCLUDE_DIRS}
        )
    
        find_library(KEXIV2_LIBRARIES NAMES kexiv2
        PATHS
        ${_pc_KEXIV2_LIBRARY_DIRS}
        )
    
        if (KEXIV2_INCLUDE_DIR AND KEXIV2_LIBRARIES)
            set(KEXIV2_FOUND TRUE)
        endif (KEXIV2_INCLUDE_DIR AND KEXIV2_LIBRARIES)
      endif(_pc_KEXIV2_FOUND)
      if (KEXIV2_FOUND)
          if (NOT Kexiv2_FIND_QUIETLY)
              message(STATUS "Found libkexiv2: ${KEXIV2_LIBRARIES}")
          endif (NOT Kexiv2_FIND_QUIETLY)
      else (KEXIV2_FOUND)
          if (Kexiv2_FIND_REQUIRED)
              if (NOT KEXIV2_INCLUDE_DIR)
                  message(FATAL_ERROR "Could NOT find libkexiv2 header files")
              endif (NOT KEXIV2_INCLUDE_DIR)
              if (NOT KEXIV2_LIBRARIES)
                  message(FATAL_ERROR "Could NOT find libkexiv2 library")
              endif (NOT KEXIV2_LIBRARIES)
          endif (Kexiv2_FIND_REQUIRED)
      endif (KEXIV2_FOUND)
    
    mark_as_advanced(KEXIV2_INCLUDE_DIR KEXIV2_LIBRARIES KEXIV2_DEFINITIONS)

  endif(KEXIV2_LOCAL_FOUND)
  
endif (KEXIV2_INCLUDE_DIR AND KEXIV2_LIBRARIES AND KEXIV2_DEFINITIONS)
