# Makefile.am - Makefile template for libhurd-mm.
# Copyright (C) 2004, 2005, 2007, 2008 Free Software Foundation, Inc.
# Written by Neal H. Walfield
#
# This file is part of the GNU Hurd.
# 
# The GNU Hurd is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Lesser Public License as
# published by the Free Software Foundation; either version 3 of the
# License, or (at your option) any later version.
#
# This GNU Hurd is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# General Lesser Public License for more details.
#
# You should have received a copy of the GNU General Lesser Public
# License along with this program.  If not, see
# <http://www.gnu.org/licenses/>.

if ARCH_IA32
  ARCH_SOURCES = ia32-exception-entry.S
endif

if ENABLE_TESTS
lib_LIBRARIES = libas-check.a
else
lib_LIBRARIES = libhurd-mm.a libas-kernel.a
endif

includehurddir = $(includedir)/hurd
includehurd_HEADERS = mm.h storage.h as.h

libhurd_mm_a_CPPFLAGS = $(USER_CPPFLAGS)
libhurd_mm_a_CCASFLAGS = $(USER_CPPFLAGS) -DASM
libhurd_mm_a_CFLAGS = $(USER_CFLAGS) -mpreferred-stack-boundary=2
libhurd_mm_a_SOURCES = mm.h			\
	bits.h					\
	capalloc.h capalloc.c			\
	storage.h storage.c			\
	as.h as.c				\
	as-build.c as-build-custom.c		\
	as-lookup.c				\
	as-dump.c				\
	exceptions.c				\
	map.h map.c				\
	pager.h pager.c				\
	anonymous.h anonymous.c			\
	mmap.c sbrk.c				\
	mm-init.c				\
	$(ARCH_SOURCES)

libas_kernel_a_CPPFLAGS = $(KERNEL_CPPFLAGS)
libas_kernel_a_CCASFLAGS = $(KERNEL_CPPFLAGS) -DASM
libas_kernel_a_CFLAGS = $(KERNEL_CFLAGS) -mpreferred-stack-boundary=2
libas_kernel_a_SOURCES = as.h bits.h as-build.c as-lookup.c as-dump.c

libas_check_a_CPPFLAGS = $(CHECK_CPPFLAGS) -DRM_INTERN
libas_check_a_CCASFLAGS = $(CHECK_CPPFLAGS) -DASM
libas_check_a_CFLAGS = $(KERNEL_CFLAGS) -mpreferred-stack-boundary=2
libas_check_a_SOURCES = as.h bits.h as-build.c as-lookup.c as-dump.c
