# Makefile.am - Makefile template for wortel.
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
  ARCH_SOURCES = multiboot.h ia32-crt0.S ia32-cmain.c \
    ia32-output.c output-vga.c ia32-shutdown.c
endif

noinst_PROGRAMS = wortel

wortel_CFLAGS = -I$(srcdir) $(AM_CFLAGS)

wortel_SOURCES = $(ARCH_SOURCES) string.h string.c		\
	output.h output.c output-none.c				\
	shutdown.h shutdown.c					\
	elf.h loader.h loader.c					\
	wortel.h wortel.c

/* FIXME: Make linkbase configurable.  */
wortel_LDFLAGS = -u_start -e_start -N -nostdlib -Ttext=0x300000

wortel_LDADD = -lgcc