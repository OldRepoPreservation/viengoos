# Makefile.am - Makefile template for libl4.
# Copyright (C) 2003, 2004, 2005 Free Software Foundation, Inc.
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
if ARCH_POWERPC
  ARCH_SUBDIR = powerpc
endif
SUBDIRS =  $(ARCH_SUBDIR)

# Header files that must be provided by all interfaces.
common_headers := ipc.h kip.h message.h misc.h schedule.h space.h	\
	syscall.h thread.h types.h sigma0.h
# Interface-specific header files.
gnu_headers := $(common_headers) math.h pagefault.h thread-start.h
compat_headers := $(common_headers)

# Main libl4 header files.
l4_headers = features.h globals.h init.h ipc.h kip.h math.h message.h	\
	misc.h pagefault.h schedule.h space.h stubs-init.h stubs.h	\
	syscall.h thread.h thread-start.h types.h vregs.h sigma0.h	\
	arch.h

include_HEADERS = l4.h
nobase_include_HEADERS = $(addprefix l4/, $(l4_headers))	\
	$(addprefix l4/gnu/, $(gnu_headers))			\
	$(addprefix l4/compat/, $(compat_headers))
