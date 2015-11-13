# - Find libnl
# Find libnl includes and library.
# Once done this will define
#
# This module defines
#  LIBNL_FOUND - whether the libnl library was found
#  LIBNL_LIBRARIES - the libnl library
#  LIBNL_INCLUDE_DIR - the include path of the libnl library

FIND_LIBRARY(LIBNL_LIBRARY nl-3)
FIND_LIBRARY(LIBNL_GENL_LIBRARY nl-genl-3)

SET(LIBNL_LIBRARIES ${LIBNL_LIBRARY} ${LIBNL_LIBRARY})

FIND_PATH(LIBNL_INCLUDE_DIR
	NAMES
        netlink/netlink.h
	PATH_SUFFIXES libnl3
)

#INCLUDE(FindPackageHandleStandardArgs)
#FIND_PACKAGE_HANDLE_STANDARD_ARGS(Libnl DEFAULT_MSG
#	LIBNL_LIBRARY
#	LIBNL_INCLUDE_DIR)
#
#MARK_AS_ADVANCED(
#	LIBNL_INCLUDE_DIR
#	LIBNL_LIBRARY
#)

