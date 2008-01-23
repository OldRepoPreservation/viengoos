# Makefile.am - Makefile template for libhurd-slab.
# Copyright (C) 2003, 2008 Free Software Foundation, Inc.
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

lib_LIBRARIES = libhurd-slab.a

includehurddir = $(includedir)/hurd
includehurd_HEADERS = slab.h

# FIXME: Build a special libhurd-ihash.a using libc-parts for the rootservers,
# and a normal for everybody else.
AM_CPPFLAGS = -I$(LIBC)/include -I$(top_builddir)/include	\
	-I$(top_srcdir)/libc-parts
libhurd_slab_a_SOURCES = slab.h slab.c
