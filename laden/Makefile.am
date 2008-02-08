# Makefile.am - Makefile template for laden.
# Copyright (C) 2003, 2004, 2008 Free Software Foundation, Inc.
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
  ARCH_SOURCES = multiboot.h ia32-crt0.S ia32-cmain.c \
    ia32-output.c output-vga.c output-serial.c ia32-shutdown.c
endif

bootdir = $(prefix)/boot
boot_PROGRAMS = laden

laden_CPPFLAGS = -I$(srcdir) -I$(top_builddir)/include \
	-I$(top_srcdir)/libc-parts $(AM_CPPFLAGS) \
	-DS_PRINTF=printf

laden_SOURCES =  $(ARCH_SOURCES)				\
	output.h output.c output-none.c				\
	shutdown.h shutdown.c					\
	elf.h loader.h loader.c					\
	laden.h laden.c kip-fixup.c

laden_LDFLAGS = -u_start -e_start -N -nostdlib -Ttext=@HURD_LADEN_LOAD_ADDRESS@

laden_LDADD = \
	../libc-parts/libc-kernel.a \
	../newlib/libc-routines.a \
	-lgcc
