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
AC_CHECK_TOOL([AR], [ar], :)
AC_CHECK_TOOL([NM], [nm], :)
AC_PATH_PROG([SED], [sed], :)
AC_PATH_PROG([SORT], [sort], :)

# Required for building the documentation
AC_PATH_PROG([LATEX], [latex], no)
if test "x$LATEX" = xno; then
  missing_progs="$missing_progs latex"
fi
AC_PATH_PROG([DVIPS], [dvips], no)
if test "x$DVIPS" = xno; then
  missing_progs="$missing_progs dvips"
fi
AC_PATH_PROG([PS2PDF], [ps2pdf], no)
if test "x$PS2PDF" = xno; then
  missing_progs="$missing_progs ps2pdf"
fi

# Checks for libraries.
AC_MSG_CHECKING([for static GNU C library])
STATIC_GLIBC=`$CC -print-file-name=libc.a`
if test "x$STATIC_GLIBC" = xlibc.a; then
  AC_MSG_RESULT([none])
  AC_MSG_ERROR([Could not find static version of the GNU C library])
else
  AC_MSG_RESULT([$STATIC_GLIBC])
fi
AC_SUBST(STATIC_GLIBC)


# Checks for header files.

# Checks for typedefs, structures, and compiler characteristics.
AC_CANONICAL_HOST
AC_C_CONST
AC_C_VOLATILE

arch_ia32=false
arch_powerpc=false
case $host_cpu in
  i386 | i486 | i586 | i686)
    arch=ia32
    arch_ia32=true
    ;;
  powerpc)
    arch=powerpc
    arch_powerpc=true
    ;;
  *)
    AC_MSG_ERROR([unsupported architecture])
    ;;
esac
AM_CONDITIONAL([ARCH_IA32], $arch_ia32)
AM_CONDITIONAL([ARCH_POWERPC], $arch_powerpc)

# Configure components.
m4_include([laden/config.m4])
m4_include([wortel/config.m4])
m4_include([physmem/config.m4])

# Create links for header file.
m4_include([libl4/headers.m4])
m4_include([libhurd-ihash/headers.m4])
m4_include([libhurd-slab/headers.m4])
m4_include([libhurd-cap/headers.m4])

if test "x$missing_progs" != "x"; then
  AC_MSG_ERROR([The following programs were not found:$missing_progs])
fi

# Checks for library functions.
AC_CONFIG_FILES([Makefile
		 libl4/ia32/Makefile libl4/powerpc/Makefile libl4/Makefile
		 libc-parts/Makefile
		 libhurd-ihash/Makefile
		 libhurd-slab/Makefile
		 libhurd-cap/Makefile
                 laden/Makefile
                 wortel/Makefile
                 physmem/Makefile
		 doc/Makefile])
AC_OUTPUT
