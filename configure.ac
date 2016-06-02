#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.63])
AC_INIT([wolpd], [0.5.0], [federico.simoncelli@gmail.com])
AM_INIT_AUTOMAKE()
AC_CONFIG_SRCDIR([wolpd.c])
AC_CONFIG_HEADERS([config.h])

# Checks for programs.
AC_PROG_CC

# Configure default CFLAGS.

# Checks for libraries.

# Checks for header files.
AC_CHECK_HEADERS([arpa/inet.h fcntl.h netinet/in.h stdlib.h string.h sys/ioctl.h sys/socket.h syslog.h unistd.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_TYPE_SIZE_T
AC_TYPE_SSIZE_T
AC_TYPE_UINT16_T
AC_TYPE_UINT8_T

# Checks for library functions.
AC_CHECK_FUNCS([inet_ntoa memset socket])

AC_CONFIG_FILES([Makefile wolpd.init])
AC_OUTPUT
