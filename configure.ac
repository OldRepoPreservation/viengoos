# configure.ac - Configure script for the GNU Hurd.
# Copyright (C) 2003, 2004, 2005, 2007, 2008 Free Software Foundation, Inc.
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
AC_CONFIG_AUX_DIR([build-aux])
AM_INIT_AUTOMAKE(
dnl Don't define `PACKAGE' and `VERSION'.
  [no-define]
dnl Do not clutter the main build directory.
  [subdir-objects]
dnl This is only a very rough estimation of the version we actually need.  Feel
dnl free to refine that.
  [1.9]
dnl We require GNU make.
  [-Wall -Wno-portability]
)
AC_CONFIG_SRCDIR([laden/laden.c])
AC_CONFIG_HEADER([config.h])
AC_GNU_SOURCE

# Checks for programs.
AC_PROG_CC
AM_PROG_CC_C_O

AM_PROG_AS
AC_PROG_RANLIB
AC_CHECK_TOOL([AR], [ar], :)
AC_CHECK_TOOL([NM], [nm], :)
AC_PATH_PROG([SED], [sed], :)
AC_PATH_PROG([SORT], [sort], :)
AC_PROG_LN_S

# Required for building the documentation
missing_doc_progs=
AC_PATH_PROG([PDFLATEX], [pdflatex], no)
if test "x$PDFLATEX" = xno; then
  missing_doc_progs="$missing_doc_progs pdflatex"
fi
AC_PATH_PROG([BIBTEX], [bibtex], no)
if test "x$BIBTEX" = xno; then
  missing_doc_progs="$missing_doc_progs bibtex"
fi
AC_PATH_PROG([INKSCAPE], [inkscape], no)
if test "x$INKSCAPE" = xno; then
  missing_doc_progs="$missing_doc_progs inkscape"
fi
DOC=
if test x"$missing_doc_progs" != "x"; then
  AC_MSG_WARN([[The following programs were not found:$missing_doc_progs.  ]]dnl
[[No documentation files will be built.]])
else
  DOC=doc
fi
AC_SUBST([DOC])


# Check for program specialities.

# From glibc HEAD, 2007-11-07.
AC_CACHE_CHECK(for -fgnu89-inline, libc_cv_gnu89_inline, [dnl
cat > conftest.c <<EOF
int foo;
#ifdef __GNUC_GNU_INLINE__
main () { return 0;}
#else
#error
#endif
EOF
if AC_TRY_COMMAND([${CC-cc} $CFLAGS $CPPFLAGS -S -std=gnu99 -fgnu89-inline
			    -o conftest.s conftest.c 1>&AS_MESSAGE_LOG_FD])
then
  libc_cv_gnu89_inline=yes
else
  libc_cv_gnu89_inline=no
fi
rm -f conftest*])
if test $libc_cv_gnu89_inline = yes; then
  libc_cv_gnu89_inline=-fgnu89-inline
else
  libc_cv_gnu89_inline=
fi


# Set up the host information.

AC_CANONICAL_HOST


# Checks for header files.

arch_ia32=false
arch_powerpc=false
case $host_cpu in
  alpha)
    arch=alpha
    arch_alpha=true
    ;;
  amd64)
    arch=amd64
    arch_amd64=true
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

# Create links for header and library files.
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
m4_include([newlib/headers.m4])
m4_include([libc-parts/headers.m4])

# Check for libc and components using it.

AC_ARG_WITH([libc],
[  --with-libc             compile with the GNU C library (see libc/README)])
AM_CONDITIONAL(WITH_LIBC, test x"${with_libc-no}" != xno)

AC_ARG_WITH([newlib],
[  --with-newlib          compile with Newlib's C library (see newlib/README)])
AM_CONDITIONAL(WITH_NEWLIB, test x"${with_newlib-no}" != xno)

ABS_BUILDDIR=$ac_pwd
AC_SUBST([ABS_BUILDDIR])
ABS_SRCDIR=`readlink -f $srcdir`
AC_SUBST([ABS_SRCDIR])
# Location of the installed headers and .a's.  We expect that
# LIBC/libc.a and LIBC/include exists.
LIBC=
if test x"${with_libc-no}" != xno
then
	LIBC=$ABS_BUILDDIR/libc
elif test x"${with_newlib-no}" != xno
then
	LIBC=$ABS_BUILDDIR/newlib
fi
AC_SUBST([LIBC])

LIBC_A=
LIBC_M=
if test x$LIBC != x
then
  LIBC_A=$LIBC/libc.a
  LIBC_M=$LIBC/libm.a
fi
AC_SUBST([LIBC_A])
AC_SUBST([LIBC_M])

# Compute some variables that we need everywhere.

USER_CPPFLAGS=" \
	-D_GNU_SOURCE \
	-I${LIBC}/include \
	-I${ABS_SRCDIR}/libc-parts"
USER_CFLAGS="-std=gnu99 -Wall -g -O2 -fno-stack-protector $libc_cv_gnu89_inline"
USER_LDFLAGS="-u_start -e_start"

KERNEL_CPPFLAGS="\
	-D_GNU_SOURCE -DRM_INTERN \
	-I${ABS_SRCDIR}/libc-parts"
KERNEL_CFLAGS="-std=gnu99 -Wall -g -O2 -fno-stack-protector $libc_cv_gnu89_inline"

# It is debatable whether these should really contain the libc-parts' include
# path.  Currently this is needed, as tests make use of (a few) specific
# libc-parts macros etc.
CHECK_CPPFLAGS="\
	-D_GNU_SOURCE \
	-D_ENABLE_TESTS \
	-I${ABS_BUILDDIR}/sysroot/include -I${ABS_SRCDIR}/libc-parts"
CHECK_CFLAGS="-std=gnu99 -Wall -g -O2 -fno-stack-protector $libc_cv_gnu89_inline"

AC_SUBST(USER_CPPFLAGS)
AC_SUBST(USER_CFLAGS)
AC_SUBST(USER_LDFLAGS)
AC_SUBST(USER_LDADD)
AC_SUBST(KERNEL_CPPFLAGS)
AC_SUBST(KERNEL_CFLAGS)
AC_SUBST(CHECK_CPPFLAGS)
AC_SUBST(CHECK_CFLAGS)


# Do we already have a cross compiler?
have_cross="test -x bin/$host_alias-gcc"
AM_CONDITIONAL([HAVE_CROSS], [[$have_cross]])

# Are we supposed to run the test suite only?
AC_ARG_WITH([tests],
[  --enable-tests          only support running the test suite)])
AM_CONDITIONAL([ENABLE_TESTS], [[test x"${enable_tests-no}" != xno]])

AM_CONDITIONAL([TRAVERSE_HIERARCHY], [[$have_cross || test x"${enable_tests-no}" != xno]])


# Checks for library functions.
AC_CONFIG_FILES([Makefile
		 sysroot/lib/libc.a:libc.a.in
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
		 newlib/Makefile
		 hieronymus/Makefile
		 ruth/Makefile
		 benchmarks/Makefile
		 benchmarks/boehm-gc/Makefile
		 doc/Makefile
		 ])
AC_OUTPUT
