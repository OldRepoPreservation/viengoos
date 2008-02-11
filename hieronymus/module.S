/* module.S - Binary packager.
   Copyright (C) 2008 Free Software Foundation, Inc.
   Written by Neal H. Walfield <neal@gnu.org>.

   This file is part of the GNU Hurd.

   The GNU Hurd is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 3 of the
   License, or (at your option) any later version.

   The GNU Hurd is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see
   <http://www.gnu.org/licenses/>.  */

#ifndef MODULE
#error "Module not defined."
#endif

#define C_(a, b) a ## b
#define C(a, b) C_(a, b)

#define S_(id) #id
#define S(id) S_(id)

#define MODULE_BINARY MODULE

	.data

	/* Include the binary.  Make it page aligned.  */
	.global C(MODULE, _start), C(MODULE, _end)
C(MODULE, _start):
	.incbin S(MODULE_BINARY)
C(MODULE, _end):
