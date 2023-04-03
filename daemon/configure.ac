# -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.69])
AC_INIT([parled12], [1.0], [vladimir@stupin.su])
AC_CONFIG_SRCDIR([main.c])
AM_INIT_AUTOMAKE([foreign -Wportability -Wall -Werror -Wsyntax])
AC_CONFIG_FILES([Makefile])

# Checks for programs.
AC_PROG_CC

# Checks for libraries.

# Checks for header files.
AC_CHECK_HEADERS([ctype.h errno.h fcntl.h grp.h limits.h pwd.h signal.h stdarg.h stdio.h stdlib.h string.h syslog.h unistd.h linux/parport.h linux/ppdev.h sys/epoll.h sys/file.h sys/ioctl.h sys/socket.h sys/stat.h sys/un.h sys/wait.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_TYPE_PID_T
AC_TYPE_SIZE_T
AC_TYPE_SSIZE_T

# Checks for library functions.
AC_FUNC_CHOWN
AC_FUNC_FORK
AC_FUNC_MALLOC
AC_FUNC_REALLOC
AC_CHECK_FUNCS([memmove socket strerror strtol strtoul])

AC_OUTPUT
