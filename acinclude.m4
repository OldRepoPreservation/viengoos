# acinclude.m4 - Macro definitions
# Copyright (C) 2003 Free Software Foundation, Inc.
# Written by Marcus Brinkmann.
# 
# This file is part of the GNU Hurd.
# 
# The GNU Hurd is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License as
# published by the Free Software Foundation; either version 2, or (at
# your option) any later version.
# 
# The GNU Hurd is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# General Public License for more details.
# 
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA.

# HURD_LOAD_ADDRESS([module], [default-link-address])
#
# For the module MODULE, set the default link address to
# DEFAULT-LINK-ADDRESS, and add an option --with-MODULE-load-address.

AC_DEFUN([HURD_LOAD_ADDRESS],
	[AC_ARG_WITH([$1-loadaddr],
	AC_HELP_STRING([--with-$1-loadaddr],
	[$1 load address @<:@$2@:>@]),
	hurd_$1_load_address=$withval,
	hurd_$1_load_address=$2)
	HURD_[]translit($1, [a-z], [A-Z])_LOAD_ADDRESS=$hurd_$1_load_address
	AC_SUBST(HURD_[]translit($1, [a-z], [A-Z])_LOAD_ADDRESS)])
