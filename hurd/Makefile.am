# Makefile.am - Makefile template for hurd.
# Copyright (C) 2004, 2007 Free Software Foundation, Inc.
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

COMMON_CPPFLAGS = -I$(srcdir) -I$(top_builddir)/include \
	-I$(top_srcdir)/libc-parts -D_GNU_SOURCE -Wall -std=gnu99 -g

includehurddir = $(includedir)/hurd
includehurd_HEADERS = startup.h stddef.h types.h addr.h \
	addr-trans.h cap.h folio.h

TESTS = t-addr t-addr-trans
check_PROGRAMS = $(TESTS)

t_addr_CPPFLAGS = $(COMMON_CPPFLAGS)
t_addr_SOURCES = t-addr.c

t_addr_trans_CPPFLAGS = $(COMMON_CPPFLAGS)
t_addr_trans_SOURCES = t-addr-trans.c

