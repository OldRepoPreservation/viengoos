# Makefile.am - Makefile template for viengoos.
# Copyright (C) 2003, 2004, 2007, 2008 Free Software Foundation, Inc.
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
if ! ENABLE_TESTS
boot_PROGRAMS = viengoos

all-local: viengoos.stripped
viengoos.stripped: viengoos
	strip -o $@ $<
endif

# We need the libc headers for errno.h -I$(LIBC)/include 
viengoos_CPPFLAGS = $(KERNEL_CPPFLAGS)
viengoos_CFLAGS = $(KERNEL_CFLAGS)
viengoos_SOURCES = $(ARCH_SOURCES)		\
	output.h output.c output-none.c		\
	debug.h debug.c				\
	shutdown.h shutdown.c			\
	panic.c					\
	sigma0.h sigma0.c			\
	zalloc.h zalloc.c			\
	rm.h					\
	viengoos.h viengoos.c			\
	boot-modules.h boot-modules.c		\
	memory.h memory.c			\
	object.h object.c			\
	cap.h cap.c 				\
	activity.h activity.c			\
	thread.h thread.c			\
	messenger.h messenger.c			\
	ager.h ager.c				\
	bits.h					\
	server.h server.c			\
	pager.h pager.c				\
	list.h

viengoos_LDADD = \
	../libhurd-btree/libhurd-btree.a \
	../libhurd-ihash/libhurd-ihash-nomalloc.a \
	../libhurd-mm/libas-kernel.a \
	../libc-parts/libc-kernel.a \
	../libhurd-mm/libas-kernel.a \
	../newlib/libc-routines.a \
	-lgcc
viengoos_LDFLAGS = -u_start -e_start -N -nostdlib \
	-Ttext=@HURD_RM_LOAD_ADDRESS@

TESTS = t-as t-activity t-link t-guard
check_PROGRAMS = $(TESTS)

CHECK_CPPFLAGS += \
	-DRM_INTERN \
	-include $(srcdir)/t-environment.h
CHECK_LDADD = \
	../libhurd-btree/libhurd-btree.a \
	../libhurd-ihash/libhurd-ihash-nomalloc.a

t_as_CPPFLAGS = $(CHECK_CPPFLAGS)
t_as_CFLAGS = $(CHECK_CFLAGS)
t_as_SOURCES = t-as.c				\
	zalloc.h zalloc.c			\
	memory.h memory.c			\
	cap.h cap.c 				\
	object.h object.c			\
	activity.h activity.c			\
	thread.h thread.c			\
	output.h output.c output-stdio.c	\
	shutdown.h shutdown.c			\
	panic.c					\
	debug.h debug.c
t_as_LDADD = ../libhurd-mm/libas-check.a $(CHECK_LDADD)

t_activity_CPPFLAGS = $(CHECK_CPPFLAGS)
t_activity_CFLAGS = $(CHECK_CFLAGS)
t_activity_SOURCES = t-activity.c		\
	zalloc.h zalloc.c			\
	memory.h memory.c			\
	cap.h cap.c 				\
	object.h object.c			\
	activity.h activity.c			\
	thread.h thread.c			\
	output.h output.c output-stdio.c	\
	shutdown.h shutdown.c			\
	panic.c					\
	debug.h debug.c
t_activity_LDADD = ../libhurd-mm/libas-check.a $(CHECK_LDADD)

t_link_CPPFLAGS = $(CHECK_CPPFLAGS)
t_link_CFLAGS = $(CHECK_CFLAGS)
t_link_SOURCES = t-link.c list.h		\
	output.h output.c output-stdio.c panic.c shutdown.h shutdown.c

t_guard_CPPFLAGS = $(CHECK_CPPFLAGS)
t_guard_CFLAGS = $(CHECK_CFLAGS)
t_guard_SOURCES = t-guard.c			\
	output.h output.c output-stdio.c panic.c shutdown.h shutdown.c

