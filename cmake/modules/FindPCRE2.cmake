# - Try to find the PCRE regular expression library
# Once done this will define
#
#  PCRE2_FOUND - system has the PCRE2 library
#  PCRE2_INCLUDE_DIR - the PCRE2 include directory
#  PCRE2_LIBRARIES - The libraries needed to use PCRE2

# Copyright (c) 2006, Alexander Neundorf, <neundorf@kde.org>
#
# Redistribution and use is allowed according to the terms of the BSD license.
# For details see the accompanying COPYING-CMAKE-SCRIPTS file.


if (PCRE2_INCLUDE_DIR AND PCRE2_PCREPOSIX_LIBRARY AND PCRE2_PCRE_LIBRARY)
  # Already in cache, be silent
  set(PCRE2_FIND_QUIETLY TRUE)
endif (PCRE2_INCLUDE_DIR AND PCRE2_PCREPOSIX_LIBRARY AND PCRE2_PCRE_LIBRARY)


if (NOT WIN32)
  # use pkg-config to get the directories and then use these values
  # in the FIND_PATH() and FIND_LIBRARY() calls
  find_package(PkgConfig)

  pkg_check_modules(PC_PCRE2 QUIET libpcre2-8)

  set(PCRE2_DEFINITIONS ${PC_PCRE2_CFLAGS_OTHER})

endif (NOT WIN32)

find_path(PCRE2_INCLUDE_DIR pcre2.h
          HINTS ${PC_PCRE2_INCLUDEDIR} ${PC_PCRE2_INCLUDE_DIRS} 
          PATH_SUFFIXES pcre2)

find_library(PCRE2_PCRE_LIBRARY NAMES pcre2-8 pcre2-8d HINTS ${PC_PCRE2_LIBDIR} ${PC_PCRE2_LIBRARY_DIRS})

find_library(PCRE2_PCREPOSIX_LIBRARY NAMES pcre2-posix pcre2-posixd HINTS ${PC_PCRE2_LIBDIR} ${PC_PCRE2_LIBRARY_DIRS})

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(PCRE2 DEFAULT_MSG PCRE2_INCLUDE_DIR PCRE2_PCRE_LIBRARY PCRE2_PCREPOSIX_LIBRARY )

set(PCRE2_LIBRARIES ${PCRE2_PCRE_LIBRARY} ${PCRE2_PCREPOSIX_LIBRARY})

mark_as_advanced(PCRE2_INCLUDE_DIR PCRE2_LIBRARIES PCRE2_PCREPOSIX_LIBRARY PCRE2_PCRE_LIBRARY)
