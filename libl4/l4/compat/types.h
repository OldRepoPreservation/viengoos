/* l4/compat/types.h - Public interface for L4 compatibility types.
   Copyright (C) 2003, 2004 Free Software Foundation, Inc.
   Written by Marcus Brinkmann <marcus@gnu.org>.

   This file is part of the GNU L4 library.
 
   The GNU L4 library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public License
   as published by the Free Software Foundation; either version 2.1 of
   the License, or (at your option) any later version.
 
   The GNU L4 library is distributed in the hope that it will be
   useful, but WITHOUT ANY WARRANTY; without even the implied warranty
   of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU Lesser General Public License for more details.
 
   You should have received a copy of the GNU Lesser General Public
   License along with the GNU L4 library; if not, write to the Free
   Software Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
   02111-1307, USA.  */

#ifndef _L4_TYPES_H
# error "Never use <l4/compat/types.h> directly; include <l4/types.h> instead."
#endif

typedef _L4_word_t L4_Word_t;
typedef _L4_uint16_t L4_Word16_t;
typedef _L4_uint64_t L4_Word64_t;
typedef _L4_word_t L4_Bool_t;


typedef struct
{
  L4_Word_t raw;
} L4_Fpage_t;
