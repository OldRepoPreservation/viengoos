# Makefile.am - Makefile template for libhurd-mm.
# Copyright (C) 2004, 2005, 2007 Free Software Foundation, Inc.
# Written by Neal H. Walfield
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

if ARCH_IA32
  ARCH_SOURCES = ia32-exception-entry.S
endif

lib_LIBRARIES = libhurd-mm.a

includehurddir = $(includedir)/hurd
includehurd_HEADERS = mm.h storage.h as.h

COMMON_CPPFLAGS = -I$(top_builddir)/include -I$(top_srcdir)/libc-parts \
	-D_GNU_SOURCE -Wall -std=gnu99 -g -O3
libhurd_mm_a_CPPFLAGS = $(COMMON_CPPFLAGS)
libhurd_mm_a_SOURCES = mm.h			\
	capalloc.h capalloc.c			\
	storage.h storage.c			\
	as.h as.c				\
	exceptions.c				\
	pager.h pager.c				\
	anonymous.h anonymous.c			\
	mmap.c					\
	mm-init.c				\
	$(ARCH_SOURCES)
