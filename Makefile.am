# Makefile.am - Makefile template for hurd-l4.
# Copyright (C) 2003, 2004, 2005, 2007, 2008 Free Software Foundation, Inc.
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

if WITH_LIBC
  LIBC_SUBDIRS = libc
else
  LIBC_SUBDIRS =
endif

if WITH_NEWLIB
  NEWLIB_SUBDIRS = newlib
else
  NEWLIB_SUBDIRS =
endif

if USE_L4
  L4_SUBDIRS = libl4 laden
else
  L4_SUBDIRS =
endif

if TRAVERSE_HIERARCHY
SUBDIRS = libviengoos platform				\
	$(LIBC_SUBDIRS)	$(NEWLIB_SUBDIRS)		\
	libhurd-ihash libhurd-btree libbitarray		\
	hurd						\
	libc-parts					\
	$(L4_SUBDIRS)					\
	libhurd-slab					\
	libpthread					\
	libhurd-mm					\
	viengoos					\
	.						\
	ruth						\
	benchmarks					\
	hieronymus					\
	$(DOC)
endif

if HAVE_CROSS

# libstdc++
all-local: libc-stmp
libc-stmp: @LIBC_A@ @LIBC_M@ \
      libhurd-mm/libhurd-mm.a \
      libpthread/libpthread.a \
      libhurd-slab/libhurd-slab.a \
      libhurd-ihash/libhurd-ihash.a \
      libhurd-btree/libhurd-btree.a \
      libc-parts/libc-parts.a
	touch libc-stmp sysroot/lib/libc.a

else

if ! ENABLE_TESTS
all-local: cross-compiler
endif

endif

.PHONY: cross-compiler
cross-compiler: binutils/binutils.build/install.stamp gcc/gcc.build/install.stamp
# Now that the cross compiler is in place, we can make the build system use it.
	if grep -q '^CC = $(host_alias)-gcc$$' < Makefile; then :; else \
	  ./config.status --recheck; \
	fi
	@echo 'The cross compiler is now set-up.  Re-run `make'\' \
	  'and proceed as usual.'

include Makefoo.am
include binutils/Makefrag.am
include gcc/Makefrag.am
