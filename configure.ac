# configure.ac - Configure script for the GNU Hurd.
# Copyright (C) 2003, 2004, 2005, 2007 Free Software Foundation, Inc.
# Written by Marcus Brinkmann.
#
# This file is part of the GNU Hurd.
# 
# The GNU Hurd is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.
#
#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ(2.57)
AC_INIT(hurd, 0.0, l4-hurd@gnu.org)
AM_INIT_AUTOMAKE
AM_MAINTAINER_MODE
AC_CONFIG_SRCDIR([laden/laden.c])
AC_CONFIG_HEADER([config.h])
AC_GNU_SOURCE

maintainer_mode=${enable_maintainer_mode-no}

# Checks for programs.
AC_PROG_CC

# FIXME: We should support setting the CFLAGS by the user.  But
# -std=gnu99 and -O2 are required.
CFLAGS="-std=gnu99 -Wall -g -O3 $CFLAGS"
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
AC_PATH_PROG([FIG2DEV], [fig2dev], no)
if test "x$FIG2DEV" = xno; then
  if test "x$maintainer_mode" = xyes; then
    missing_progs="missing_progs fig2dev"
  fi
fi

# Set up the host information.

AC_CANONICAL_HOST

# Checks for libraries.

AC_MSG_CHECKING([for static GNU C library])
static_libc=libc.a
case $host_os in
  gnu*)
    # On the GNU/Hurd, libc.a is a linker script, the real C library
    # can be found in libcrt.a instead.  But fall back on libc.a for
    # cheap cross-compilation.
    libcrta=`$CC -print-file-name=libcrt.a`
    if test "x$libcrta" != "xlibcrt.a"; then
      static_libc=libcrt.a
    fi
    ;;
esac
STATIC_GLIBC=`$CC -print-file-name=$static_libc`
if test "x$STATIC_GLIBC" = "x$static_libc"; then
  AC_MSG_RESULT([none])
  AC_MSG_ERROR([Could not find static version of the GNU C library])
else
  AC_MSG_RESULT([$STATIC_GLIBC])
fi
AC_SUBST(STATIC_GLIBC)


# Checks for header files.

# Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST
AC_C_VOLATILE

arch_ia32=false
arch_powerpc=false
case $host_cpu in
  alpha)
    arch=alpha
    arch_alpha=true
    ;;
  amd64)
    arch=amd64
    arch_alpha=true
    ;;
  i386 | i486 | i586 | i686)
    arch=ia32
    arch_ia32=true
    ;;
  ia64)
    arch=ia64
    arch_ia64=true
    ;;
  powerpc)
    arch=powerpc
    arch_powerpc=true
    ;;
  powerpc64)
    arch=powerpc64
    arch_powerpc64=true
    ;;
  *)
    AC_MSG_ERROR([unsupported architecture])
    ;;
esac
AM_CONDITIONAL([ARCH_ALPHA], $arch_alpha)
AM_CONDITIONAL([ARCH_AMD64], $arch_amd64)
AM_CONDITIONAL([ARCH_IA32], $arch_ia32)
AM_CONDITIONAL([ARCH_IA64], $arch_ia64)
AM_CONDITIONAL([ARCH_POWERPC], $arch_powerpc)
AM_CONDITIONAL([ARCH_POWERPC64], $arch_powerpc64)

l4_abi_v2=false
l4_abi_x2=false
AC_ARG_ENABLE([l4-abi],
	      AC_HELP_STRING([--enable-l4-abi=abi],
		             [L4 ABI to use (supported are: v2, x2)]),
	      [ac_cv_use_l4_abi=$enableval], [ac_cv_use_l4_abi=no])
case $ac_cv_use_l4_abi in
  v2)
    l4_abi=v2
    l4_abi_v2=true
    ;;
  x2)
    l4_abi=x2
    l4_abi_x2=true
    ;;
  no)
    AC_MSG_ERROR([L4 ABI not specified; select an ABI using --enable-l4-abi])
    ;;
  *)
    AC_MSG_ERROR([unsupported L4 ABI $ac_cv_use_l4_abi])
    ;;
esac
AM_CONDITIONAL([L4_ABI_V2], $l4_abi_v2)
AM_CONDITIONAL([L4_ABI_X2], $l4_abi_x2)
AC_MSG_NOTICE([compiling for L4 ABI $l4_abi])

# Configure components.
m4_include([laden/config.m4])
m4_include([viengoos/config.m4])

# Create links for header file.
m4_include([libl4/headers.m4])
m4_include([platform/headers.m4])
m4_include([hurd/headers.m4])
m4_include([libhurd-ihash/headers.m4])
m4_include([libhurd-btree/headers.m4])
m4_include([libbitarray/headers.m4])
m4_include([libhurd-slab/headers.m4])
m4_include([libpthread/headers.m4])
m4_include([libhurd-mm/headers.m4])
m4_include([viengoos/headers.m4])

# Check for libc and components using it.

AC_ARG_WITH([libc],
[  --with-libc             compile with the GNU C library (see libc/README)])
AM_CONDITIONAL(WITH_LIBC, test x"${with_libc-no}" != xno)


if test "x$missing_progs" != "x"; then
  AC_MSG_ERROR([The following programs were not found:$missing_progs])
fi

# Checks for library functions.
AC_CONFIG_FILES([Makefile
		 libl4/ia32/Makefile libl4/powerpc/Makefile
		 libl4/v2/Makefile libl4/v2/ia32/Makefile
		 libl4/x2/Makefile
		 libl4/tests/Makefile libl4/Makefile
		 platform/alpha/Makefile platform/amd64/Makefile
		 platform/ia32/Makefile platform/ia64/Makefile
		 platform/powerpc/Makefile platform/powerpc64/Makefile
		 platform/Makefile
		 libc-parts/Makefile
		 hurd/Makefile
		 libhurd-ihash/Makefile
		 libhurd-btree/Makefile
		 libbitarray/Makefile
		 libhurd-slab/Makefile
		 libpthread/Makefile
		 libhurd-mm/Makefile
                 laden/Makefile
		 viengoos/Makefile
		 libc/Makefile
                 ruth/Makefile
		 doc/Makefile])
AC_OUTPUT
