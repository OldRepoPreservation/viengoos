# Makefile.am - Makefile template for libl4.
# Copyright (C) 2003 Free Software Foundation, Inc.
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
  ARCH_SUBDIR = ia32
endif
SUBDIRS =  $(ARCH_SUBDIR)

include_HEADERS = l4.h
nobase_include_HEADERS = l4/types.h l4/math.h l4/globals.h l4/init.h	\
	l4/vregs.h l4/stubs.h l4/stubs-init.h l4/syscall.h		\
	l4/kip.h l4/thread.h l4/schedule.h l4/space.h			\
	l4/ipc.h l4/misc.h
