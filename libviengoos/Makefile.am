# Makefile.am - Makefile template for libviengoos.
# Copyright (C) 2008 Free Software Foundation, Inc.
# Written by Neal H. Walfield
#
# This file is part of the GNU Hurd.
# 
# The GNU Hurd is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 3 of the License, or
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

viengoos_headers = addr.h addr-trans.h cap.h			\
	thread.h folio.h activity.h futex.h messenger.h		\
	message.h ipc.h						\
	rpc.h							\
	misc.h

nobase_include_HEADERS = viengoos.h				\
	$(addprefix viengoos/, $(viengoos_headers))

TESTS = t-addr t-addr-trans t-rpc
check_PROGRAMS = $(TESTS)

t_addr_CPPFLAGS = $(CHECK_CPPFLAGS)
t_addr_SOURCES = t-addr.c

t_addr_trans_CPPFLAGS = $(CHECK_CPPFLAGS)
t_addr_trans_SOURCES = t-addr-trans.c

t_rpc_CPPFLAGS = $(CHECK_CPPFLAGS)
t_rpc_SOURCES = t-rpc.c
