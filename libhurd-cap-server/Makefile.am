# Makefile.am - Makefile template for libhurd-cap-server.
# Copyright (C) 2004 Free Software Foundation, Inc.
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

lib_LIBRARIES = libhurd-cap-server.a

includehurddir = $(includedir)/hurd
includehurd_HEADERS = cap-server.h

# FIXME: Build a special libhurd-ihash.a using libc-parts for the rootservers,
# and a normal for everybody else.
AM_CPPFLAGS = -I$(top_builddir)/include -I$(top_srcdir)/libc-parts
AM_CFLAGS = -std=gnu99
libhurd_cap_server_a_SOURCES = table.h table.c				\
	task-death.h task-death.c					\
	cap-server-intern.h class-init.c class-create.c			\
	class-destroy.c class-free.c					\
	class-alloc.c class-inhibit.c					\
	obj-dealloc.c obj-drop.c obj-inhibit.c				\
	obj-entry-space.c obj-copy-out.c				\
	client-create.c client-release.c client-inhibit.c		\
	bucket-create.c bucket-free.c bucket-inhibit.c			\
	bucket-manage-mt.c bucket-inject.c
