# Makefile.am - Makefile template for libc-parts.
# Copyright (C) 2003, 2008 Free Software Foundation, Inc.
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
  ARCH_COMMON_SOURCES = ia32-setjmp.S 
  ARCH_USER_SOURCES = ia32-cmain.c ia32-crt0.S
  ARCH_KERNEL_SOURCES =
endif

if ! ENABLE_TESTS
noinst_LIBRARIES = libc-parts.a libc-kernel.a
endif

common_sources = assert.h 					\
	strtol.c strtoll.c strtoul.c strtoull.c			\
	setjmp.h backtrace.c s_printf.c startup.c		\
	panic.c _exit.c						\
	elf.h loader.h loader.c					\
	process-spawn.h process-spawn.c

libc_parts_a_CPPFLAGS = $(USER_CPPFLAGS)
libc_parts_a_CFLAGS = $(USER_CFLAGS)
libc_parts_a_SOURCES = 						\
	$(ARCH_COMMON_SOURCES) $(ARCH_USER_SOURCES) 		\
	ctype.h c-ctype.h c-ctype.c				\
	getpagesize.c		 				\
	$(common_sources)

libc_kernel_a_CPPFLAGS = $(KERNEL_CPPFLAGS)
libc_kernel_a_CFLAGS = $(KERNEL_CFLAGS)
libc_kernel_a_SOURCES =						\
	$(ARCH_COMMON_SOURCES) $(ARCH_KERNEL_SOURCES) 		\
	errno.h errno.c	 					\
	$(common_sources)

TESTS = t-setjmp
check_PROGRAMS = $(TESTS)

t_setjmp_CPPFLAGS = $(CHECK_CPPFLAGS)
t_setjmp_SOURCES = t-setjmp.c setjmp.h ia32-setjmp.S
