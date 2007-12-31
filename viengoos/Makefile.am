# Makefile.am - Makefile template for viengoos.
# Copyright (C) 2003, 2004, 2007 Free Software Foundation, Inc.
# Written by Marcus Brinkmann.
#
# This file is part of the GNU Hurd.
# 
# The GNU Hurd is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 3 of the License, or
# (at your option) any later version.
#
# This GNU Hurd is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.

if ARCH_IA32
  ARCH_SOURCES = multiboot.h ia32-crt0.S ia32-cmain.c \
    ia32-output.c output-vga.c output-serial.c ia32-shutdown.c
endif

bootdir = $(prefix)/boot
boot_PROGRAMS = viengoos

COMMON_CPPFLAGS = -I$(srcdir) -I$(top_builddir)/include \
	-I$(top_srcdir)/libc-parts -D_GNU_SOURCE -Wall -std=gnu99 -g

viengoos_CPPFLAGS = $(COMMON_CPPFLAGS) -DRM_INTERN
viengoos_SOURCES = $(ARCH_SOURCES)		\
	output.h output.c output-none.c		\
	debug.h debug.c				\
	shutdown.h shutdown.c			\
	panic.c					\
	sigma0.h sigma0.c			\
	malloc.h malloc-wrap.c			\
	zalloc.h zalloc.c			\
	mmap.c					\
	rm.h					\
	viengoos.h viengoos.c			\
	boot-modules.h boot-modules.c		\
	memory.h memory.c			\
	object.h object.c			\
	cap.h cap.c cap-lookup.c		\
	activity.h activity.c			\
	thread.h thread.c			\
	ager.h ager.c				\
	as.h as.c				\
	bits.h					\
	elf.h loader.h loader.c			\
	server.h server.c			\
	list.h

# Doug Lea's malloc is included by malloc-wrap.c.
EXTRA_viengoos_SOURCES = malloc.c

viengoos_LDADD = -lgcc ../libc-parts/libc-parts.a \
	../libhurd-btree/libhurd-btree.a \
	../libhurd-ihash/libhurd-ihash.a
viengoos_LDFLAGS = -u_start -e_start -N -nostdlib \
	-Ttext=@HURD_RM_LOAD_ADDRESS@

TESTS = t-as t-activity t-link t-guard
check_PROGRAMS = $(TESTS)

t_as_CPPFLAGS = $(viengoos_CPPFLAGS) -include $(srcdir)/t-environment.h
t_as_SOURCES = t-as.c				\
	zalloc.h zalloc.c			\
	memory.h memory.c			\
	cap.h cap.c cap-lookup.c		\
	object.h object.c			\
	activity.h activity.c			\
	thread.h thread.c			\
	as.h as.c				\
	output.h output.c output-stdio.c	\
	shutdown.h shutdown.c			\
	panic.c					\
	debug.h debug.c
t_as_LDADD = $(viengoos_LDADD)

t_activity_CPPFLAGS = $(viengoos_CPPFLAGS) -include $(srcdir)/t-environment.h
t_activity_SOURCES = t-activity.c		\
	zalloc.h zalloc.c			\
	memory.h memory.c			\
	cap.h cap.c cap-lookup.c		\
	object.h object.c			\
	activity.h activity.c			\
	thread.h thread.c			\
	as.h as.c				\
	output.h output.c output-stdio.c	\
	shutdown.h shutdown.c			\
	panic.c					\
	debug.h debug.c
t_activity_LDADD = $(viengoos_LDADD)

t_link_CPPFLAGS = $(viengoos_CPPFLAGS) -include $(srcdir)/t-environment.h
t_link_SOURCES = t-link.c list.h		\
	output.h output.c output-stdio.c panic.c shutdown.h shutdown.c

t_guard_CPPFLAGS = $(viengoos_CPPFLAGS) -include $(srcdir)/t-environment.h
t_guard_SOURCES = t-guard.c			\
	output.h output.c output-stdio.c panic.c shutdown.h shutdown.c

lib_LIBRARIES = libhurd-cap.a
libhurd_cap_a_CPPFLAGS = $(COMMON_CPPFLAGS)
libhurd_cap_a_SOURCES = cap-lookup.c as.c as.h

