/* ctype.h - Simpe ctype.h replacement for libc-parts.
   Copyright (C) 2003 Free Software Foundation, Inc.
   Written by Marcus Brinkmann.

   This file is part of the GNU Hurd.

   The GNU Hurd is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.

   The GNU Hurd is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with the GNU C Library; if not, write to the Free
   Software Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
   02111-1307 USA.  */

#ifndef CTYPE_H
#define CTYPE_H

#define isascii c_isascii
#define isalnum c_isalnum
#define isalpha c_isalpha
#define isblank c_isblank
#define iscntrl c_iscntrl
#define isdigit c_isdigit
#define islower c_islower
#define isgraph c_isgraph
#define isprint c_isprint
#define ispunct c_ispunct
#define isspace c_isspace
#define isupper c_isupper
#define isxdigi c_isxdigi
#define tolower c_tolower 
#define toupper c_toupper 

#include "c-ctype.h"

#endif /* CTYPE_H */
