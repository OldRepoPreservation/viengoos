# Makefile.am - Makefile template for hurd.
# Copyright (C) 2004, 2007, 2008 Free Software Foundation, Inc.
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

COMMON_CPPFLAGS = -I$(srcdir) -I$(top_builddir)/include \
	-I$(top_srcdir)/libc-parts -D_GNU_SOURCE -Wall -std=gnu99 -g

includehurddir = $(includedir)/hurd
includehurd_HEADERS = rpc.h startup.h stddef.h types.h addr.h \
	addr-trans.h cap.h folio.h exceptions.h thread.h activity.h \
	lock.h mutex.h

TESTS = t-addr t-addr-trans t-rpc
check_PROGRAMS = $(TESTS)

t_addr_CPPFLAGS = $(COMMON_CPPFLAGS)
t_addr_SOURCES = t-addr.c

t_addr_trans_CPPFLAGS = $(COMMON_CPPFLAGS)
t_addr_trans_SOURCES = t-addr-trans.c

t_rpc_CPPFLAGS = $(COMMON_CPPFLAGS)
t_rpc_SOURCES = t-rpc.c

