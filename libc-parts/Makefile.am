# Makefile.am - Makefile template for libc-parts.
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

noinst_LIBRARIES = libc-parts.a

libc_parts_a_SOURCES = 

routines := memcpy memmove memset bcopy bzero memchr rawmemchr \
		 strstr strtok strchr strchrnul strcpy stpcpy strcat \
		 strcmp strncmp \
		 htonl htons ntohl ntohs ffs

# We have to find all modules in the archive which contain the above
# functions.
routines_subexp := 's/^\($(shell echo $(routines) \
	| $(SED) -e 's/ \+/\\|/g')\) in \(.*\)$$/\2/p'
routines_objects := $(shell $(NM) --print-armap @STATIC_GLIBC@ 2>/dev/null \
	| $(SED) -n -e $(routines_subexp) | $(SORT) -u)

# This stupid trick seems to be necessary to avoid early expansion of
# routines_objects in Makefile.in by automake, which doesn't work at all.
routines_varname := routines_objects
libc_parts_a_LIBADD = $($(routines_varname))

$(routines_objects): %.$(OBJEXT): @STATIC_GLIBC@
	$(AR) -x $< $@
