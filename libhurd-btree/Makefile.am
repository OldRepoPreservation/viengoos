# Makefile.am - Makefile template for libhurd-btree.
# Copyright (C) 2004, 2007 Free Software Foundation, Inc.
# Written by Neal H. Walfield <neal@gnu.org>.
#
# This file is part of the GNU Hurd.
#
# The GNU Hurd is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License as
# published by the Free Software Foundation; either version 2, or (at
# your option) any later version.
# 
# The GNU Hurd is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# General Public License for more details.
# 
# You should have received a copy of the GNU General Public License
# along with the GNU Hurd; see the file COPYING.  If not, write to
# the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139,
# USA.  */

lib_LIBRARIES = libhurd-btree.a

includehurddir = $(includedir)/hurd
includehurd_HEADERS = btree.h

COMMON_CPPFLAGS = -I$(top_builddir)/include -I$(top_srcdir)/libc-parts \
	-D_GNU_SOURCE
libhurd_btree_a_CPPFLAGS = $(COMMON_CPPFLAGS) -DNCHECKS
libhurd_btree_a_SOURCES = btree.h btree.c

TESTS = btree-test

check_PROGRAMS = btree-test
btree_test_SOURCES = btree-test.c btree.h btree.c
btree_test_CPPFLAGS = -DS_PRINTF=printf $(COMMON_CPPFLAGS)

