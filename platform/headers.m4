# headers.m4 - Autoconf snippets to install links for header files.
# Copyright 2004, 2008 Free Software Foundation, Inc.
# Written by Marcus Brinkmann <marcus@gnu.org>.
#
# This file is free software; as a special exception the author gives
# unlimited permission to copy and/or distribute it, with or without
# modifications, as long as this notice is preserved.
#
# This file is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY, to the extent permitted by law; without even the
# implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

# glibc allows for more complicated sysdep constructs, which are used, e.g.,
# for the <bits/wordsize.h> case.  We'll go for a simple solution for now.
AC_CONFIG_LINKS([
  sysroot/include/atomic.h:platform/atomic.h
  sysroot/include/bits/atomic.h:platform/${arch}/bits/atomic.h
  sysroot/include/bits/wordsize.h:platform/${arch}/bits/wordsize.h
  sysroot/include/compiler.h:platform/compiler.h
  sysroot/include/endian.h:platform/endian.h
  sysroot/include/bits/endian.h:platform/${arch}/bits/endian.h
  sysroot/include/sys/io.h:platform/${arch}/sys/io.h
])
