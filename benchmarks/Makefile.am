# Makefile.am - Makefile template for hurd-l4.
# Copyright (C) 2008 Free Software Foundation, Inc.
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

bootdir = $(prefix)/boot

if ! ENABLE_TESTS
SUBDIRS = boehm-gc

boot_PROGRAMS = shared-memory-distribution activity-distribution
endif

shared_memory_distribution_CPPFLAGS = $(USER_CPPFLAGS)
shared_memory_distribution_CFLAGS = $(USER_CFLAGS)
shared_memory_distribution_LDFLAGS = $(USER_LDFLAGS)
shared_memory_distribution_LDADD = $(USER_LDADD)
shared_memory_distribution_SOURCES = shared-memory-distribution.c

activity_distribution_CPPFLAGS = $(USER_CPPFLAGS)
activity_distribution_CFLAGS = $(USER_CFLAGS)
activity_distribution_LDFLAGS = $(USER_LDFLAGS)
activity_distribution_LDADD = $(USER_LDADD)
activity_distribution_SOURCES = activity-distribution.c

