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

if ENABLE_TESTS
libhurd_ihash_a_CPPFLAGS = $(CHECK_CPPFLAGS)
libhurd_ihash_a_CFLAGS = $(CHECK_CFLAGS)
else
libhurd_ihash_a_CPPFLAGS = $(USER_CPPFLAGS)
libhurd_ihash_a_CFLAGS = $(USER_CFLAGS)
endif
libhurd_ihash_a_SOURCES = ihash.h ihash.c

libhurd_ihash_nomalloc_a_CPPFLAGS = -DNO_MALLOC
if ENABLE_TESTS
libhurd_ihash_nomalloc_a_CPPFLAGS += $(CHECK_CPPFLAGS)
libhurd_ihash_nomalloc_a_CFLAGS = $(CHECK_CFLAGS)
else
libhurd_ihash_nomalloc_a_CPPFLAGS += $(USER_CPPFLAGS)
libhurd_ihash_nomalloc_a_CFLAGS = $(USER_CFLAGS)
endif
libhurd_ihash_nomalloc_a_SOURCES = ihash.h ihash.c

TESTS = t-ihash t-ihash64
check_PROGRAMS = t-ihash t-ihash64

t_ihash_SOURCES = t-ihash.c ihash.h ihash.c
t_ihash_CPPFLAGS = $(CHECK_CPPFLAGS) \
	-DTEST_LARGE=false
t_ihash_CFLAGS=-std=gnu99

t_ihash64_SOURCES = t-ihash.c ihash.h ihash.c
t_ihash64_CPPFLAGS = $(CHECK_CPPFLAGS) \
	-DTEST_LARGE=true
t_ihash64_CFLAGS=-std=gnu99
