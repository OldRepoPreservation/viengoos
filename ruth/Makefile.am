# Makefile.am - Makefile template for the ruth server.
# Copyright (C) 2004, 2005, 2008 Free Software Foundation, Inc.
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

if ARCH_IA32
  ARCH_SOURCES = ia32-crt0.S ia32-cmain.c
endif

bootdir = $(prefix)/boot
boot_PROGRAMS = ruth

COMMON_CPPFLAGS = -Wall -g -std=gnu99 -D_GNU_SOURCE -I$(srcdir)		\
	-I$(top_builddir)/newlib/include -I$(top_builddir)/include	\
	-I$(top_srcdir)/libc-parts

ruth_CPPFLAGS = $(COMMON_CPPFLAGS)
ruth_SOURCES = $(ARCH_SOURCES)			\
	panic.h panic.c				\
	ruth.h ruth.c

ruth_LDFLAGS = -u_start -e_start -nostdlib

ruth_LDADD = $(top_builddir)/libc.a
