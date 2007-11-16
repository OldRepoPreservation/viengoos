# Makefile.am - Makefile template for hurd-l4.
# Copyright (C) 2003, 2004, 2005, 2007 Free Software Foundation, Inc.
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

SUBDIRS = libl4 platform				\
	hurd libhurd-ihash libhurd-btree libbitarray	\
	libhurd-slab					\
	libpthread					\
	libhurd-mm					\
	libc-parts laden viengoos ruth			\
	$(LIBC_SUBDIRS)					\
	doc
