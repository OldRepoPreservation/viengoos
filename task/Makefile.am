# Makefile.am - Makefile template for the task server.
# Copyright (C) 2004, 2005 Free Software Foundation, Inc.
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
boot_PROGRAMS = task

task_CPPFLAGS = -I$(top_builddir)/include			\
	-I$(top_srcdir)/libc-parts $(AM_CPPFLAGS)

task_SOURCES = $(ARCH_SOURCES)					\
	output.h output.c					\
	mmap.c malloc-wrap.c					\
	task.h task.c task-class.c task-id.c thread.c

# Doug Lea's malloc is included by malloc-wrap.c.
EXTRA_task_SOURCES = malloc.c

task_LDFLAGS = -u_start -e_start -nostdlib

task_LDADD = ../libhurd-cap-server/libhurd-cap-server.a		\
	../libhurd-mm/libhurd-mm.a				\
	../libhurd-slab/libhurd-slab.a				\
	../libpthread/libpthread.a				\
	../libhurd-ihash/libhurd-ihash.a			\
	../libhurd-btree/libhurd-btree.a			\
	../libc-parts/libc-parts.a -lgcc
