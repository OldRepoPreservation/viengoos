# Makefile.am - Makefile template for libhurd-ihash.
# Copyright (C) 2003, 2007, 2008 Free Software Foundation, Inc.
# Written by Marcus Brinkmann.
#
# This file is part of the GNU Hurd.
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public
# License as published by the Free Software Foundation; either
# version 2.1 of the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public
# License along with this program; if not, write to the Free
# Software Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
# 02111-1307 USA.

lib_LIBRARIES = libhurd-ihash.a libhurd-ihash-nomalloc.a

includehurddir = $(includedir)/hurd
includehurd_HEADERS = ihash.h

# FIXME: Build a special libhurd-ihash.a using libc-parts for the rootservers,
# and a normal for everybody else.
AM_CPPFLAGS = -I$(LIBC)/include -I$(top_builddir)/include	\
	-I$(top_srcdir)/libc-parts
AM_CFLAGS = -std=gnu99

libhurd_ihash_a_SOURCES = ihash.h ihash.c

libhurd_ihash_nomalloc_a_CPPFLAGS = $(AM_CPPFLAGS) -DNO_MALLOC
libhurd_ihash_nomalloc_a_SOURCES = ihash.h ihash.c

TESTS = t-ihash t-ihash64
check_PROGRAMS = t-ihash t-ihash64

t_ihash_SOURCES = t-ihash.c ihash.h ihash.c
t_ihash_CPPFLAGS = -DTEST_LARGE=false -DS_PRINTF=printf $(AM_CPPFLAGS)

t_ihash64_SOURCES = t-ihash.c ihash.h ihash.c
t_ihash64_CPPFLAGS = -DTEST_LARGE=true -DS_PRINTF=printf $(AM_CPPFLAGS)
