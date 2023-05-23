# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.71])
AC_INIT([sffs], [0.1], [danil.malapura@gmail.com])

# Obsolete check so I ommited it on purpose
# AC_CONFIG_SRCDIR([src/readelf.h])
AC_CONFIG_HEADERS([config.h])

# Checks for programs.
AC_PROG_CC

# Checks for libraries.
# We do need to check for a fuse

# Checks for header files.
AC_CHECK_HEADERS([include/sffs.h include/sffs_fuse.h include/sffs_direntry.h 
    include/sffs_err.h])

# Directory for weird auto-generated scripts
AC_CONFIG_AUX_DIR([build-aux])
AM_PROG_AR
AM_INIT_AUTOMAKE([-Wall -Werror foreign subdir-objects])

# Enable support for a shared library
LT_INIT([shared disable-static])
AC_CONFIG_MACRO_DIRS([m4])

# Check for a FUSE and pthread
AC_SEARCH_LIBS([fuse_main], [fuse], [], [
  AC_MSG_ERROR([unable to find fuse_main])
])
AC_CHECK_LIB(pthread, pthread_create, , [
  AC_MSG_ERROR([unable to find pthread])
])

# Checks for typedefs, structures, and compiler characteristics
AC_C_INLINE
AC_TYPE_PID_T
AC_TYPE_SIZE_T
AC_TYPE_UINT16_T
AC_TYPE_UINT32_T
AC_TYPE_UINT64_T
AC_TYPE_UINT8_T

AC_CONFIG_FILES([
    Makefile 
    src/Makefile
    utils/Makefile])
AC_OUTPUT