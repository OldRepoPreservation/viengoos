# configure.ac - Configure script for the GNU Hurd.
# Copyright (C) 2003 Free Software Foundation, Inc.
# Written by Marcus Brinkmann.
#
# This file is part of the GNU Hurd.
# 
# The GNU Hurd is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
# 
# The GNU Hurd is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
# 
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA
#
#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ(2.57)
AC_INIT(hurd, 0.0, marcus@gnu.org)
AM_INIT_AUTOMAKE
AM_MAINTAINER_MODE
AC_CONFIG_SRCDIR([laden/laden.c])
AC_CONFIG_HEADER([config.h])
AC_GNU_SOURCE

# Checks for programs.
AC_PROG_CC
AM_PROG_AS
AC_PROG_RANLIB

# Checks for libraries.

# Checks for header files.

# Checks for typedefs, structures, and compiler characteristics.
AC_CANONICAL_HOST
AC_C_CONST
AC_C_VOLATILE

case $host_cpu in
  i386 | i486 | i586 | i686)
    AM_CONDITIONAL([ARCH_IA32], true)
    ;;
  *)
    AC_MSG_ERROR([unsupported architecture])
    ;;
esac

# Create links for header file.
m4_include([libhurd-slab/headers.m4])
m4_include([libhurd-ihash/headers.m4])

# Checks for library functions.
AC_CONFIG_FILES([Makefile
                 laden/Makefile
		 libl4/ia32/Makefile libl4/Makefile
		 libhurd-slab/Makefile
		 libhurd-ihash/Makefile])
AC_OUTPUT
