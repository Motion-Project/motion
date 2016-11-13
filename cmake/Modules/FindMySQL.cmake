# Find the native MySQL includes and libraries
#
# This module defines the following variables:
# MYSQL_INCLUDE_DIRS
# MYSQL_LIBRARIES
# MYSQL_FOUND

find_path(MYSQL_INCLUDE_DIR NAMES mysql.h
  PATHS /usr/include/mysql /usr/local/include/mysql)
find_library(MYSQL_LIBRARY NAMES mysqlclient
  PATHS /usr/lib /usr/local/lib)

set(MYSQL_INCLUDE_DIRS ${MYSQL_INCLUDE_DIR})
set(MYSQL_LIBRARIES ${MYSQL_LIBRARY})

include(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(MYSQL DEFAULT_MSG MYSQL_LIBRARY MYSQL_INCLUDE_DIR)

mark_as_advanced(MYSQL_LIBRARY MYSQL_INCLUDE_DIR)
