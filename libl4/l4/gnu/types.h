/* l4/gnu/types.h - Public GNU interface for L4 types.
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
# error "Never use <l4/gnu/types.h> directly; include <l4/types.h> instead."
#endif


#define L4_BYTE_ORDER		_L4_BYTE_ORDER
#define L4_WORDSIZE		_L4_WORDSIZE

#define L4_LITTLE_ENDIAN	_L4_LITTLE_ENDIAN
#define L4_BIG_ENDIAN		_L4_BIG_ENDIAN

#define L4_WORDSIZE_32		_L4_WORDSIZE_32
#define L4_WORDSIZE_64		_L4_WORDSIZE_64


typedef _L4_int8_t l4_int8_t;
typedef _L4_int16_t l4_int16_t;
typedef _L4_int32_t l4_int32_t;
typedef _L4_int64_t l4_int64_t;

typedef _L4_uint8_t l4_uint8_t;
typedef _L4_uint16_t l4_uint16_t;
typedef _L4_uint32_t l4_uint32_t;
typedef _L4_uint64_t l4_uint64_t;


typedef _L4_word_t l4_word_t;
#define L4_WORD_C(x)	_L4_WORD_C(x)

#if _L4_WORDSIZE == _L4_WORDSIZE_32
#define __L4_PRI_PREFIX
#else
#define __L4_PRI_PREFIX	"l"
#endif

#define L4_PRIdWORD	__L4_PRI_PREFIX "d"
#define L4_PRIiWORD	__L4_PRI_PREFIX "i"
#define L4_PRIoWORD	__L4_PRI_PREFIX "o"
#define L4_PRIuWORD	__L4_PRI_PREFIX "u"
#define L4_PRIxWORD	__L4_PRI_PREFIX "x"
#define L4_PRIXWORD	__L4_PRI_PREFIX "X"

#define L4_SCNdWORD	__L4_PRI_PREFIX "d"
#define L4_SCNiWORD	__L4_PRI_PREFIX "i"
#define L4_SCNoWORD	__L4_PRI_PREFIX "o"
#define L4_SCNuWORD	__L4_PRI_PREFIX "u"
#define L4_SCNxWORD	__L4_PRI_PREFIX "x"
#define L4_SCNXWORD	__L4_PRI_PREFIX "X"


/* Basic types.  */

typedef _L4_thread_id_t l4_thread_id_t;

typedef _L4_fpage_t l4_fpage_t;
