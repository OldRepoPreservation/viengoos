/* l4/types.h - Public interface for L4 types.
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
#define _L4_TYPES_H	1

#include <l4/features.h>

/* The architecture specific file defines _L4_BYTE_ORDER and _L4_WORDSIZE.  */
#include <l4/bits/types.h>


#define _L4_LITTLE_ENDIAN	0
#define _L4_BIG_ENDIAN		1

# define __L4_intN_t(N, MODE) \
  typedef int _L4_int##N##_t __attribute__ ((__mode__ (MODE)))
# define __L4_uintN_t(N, MODE) \
  typedef unsigned int _L4_uint##N##_t __attribute__ ((__mode__ (MODE)))

__L4_intN_t (8, __QI__);
__L4_intN_t (16, __HI__);
__L4_intN_t (32, __SI__);
__L4_intN_t (64, __DI__);

__L4_uintN_t (8, __QI__);
__L4_uintN_t (16, __HI__);
__L4_uintN_t (32, __SI__);
__L4_uintN_t (64, __DI__);
#if _L4_WORDSIZE == 64
__L4_uintN_t (128, __TI__);
#endif

#if _L4_WORDSIZE == 32
typedef _L4_uint32_t _L4_word_t;
#define _L4_WORD_C(c)	c ## U
typedef _L4_uint64_t _L4_dword_t;
#else
#if _L4_WORDSIZE == 64
typedef _L4_uint64_t _L4_word_t;
#define _L4_WORD_C(c)	c ## UL
typedef _L4_uint128_t _L4_dword_t;
#else
#error "Unsupported word size."
#endif
#endif


/* Utility macros for structure definitions.  */

/* A bit-field element.  */
#define _L4_BITFIELD(name, nr) name : nr
#define _L4_BITFIELD1(type, bf1) type bf1

/* Sometimes the bit-field has different sizes depending on the word
   size, and sometimes it only exists on systems with a specific word
   size.  */
#if _L4_WORDSIZE == 32
#define _L4_BITFIELD_64(name, nr) : 0
#define _L4_BITFIELD_32_64(name, nr32, nr64) name : nr32
#else
#if _L4_WORDSIZE == 64
#define _L4_BITFIELD_64(name, nr) name : nr
#define _L4_BITFIELD_32_64(name, nr32, nr64) name : nr64
#endif
#endif

/* The representation of a bit-field is system and/or compiler
   specific.  We require that GCC uses a specific representation that
   only depends on the endianess of the system.  */
#if _L4_BYTE_ORDER == _L4_LITTLE_ENDIAN
#define _L4_BITFIELD2(type, bf1, bf2) type bf1; type bf2
#define _L4_BITFIELD3(type, bf1, bf2, bf3) type bf1; type bf2; type bf3
#define _L4_BITFIELD4(type, bf1, bf2, bf3, bf4) \
  type bf1; type bf2; type bf3; type bf4
#define _L4_BITFIELD5(type, bf1, bf2, bf3, bf4, bf5) \
  type bf1; type bf2; type bf3; type bf4; type bf5
#define _L4_BITFIELD6(type, bf1, bf2, bf3, bf4, bf5, bf6) \
  type bf1; type bf2; type bf3; type bf4; type bf5; type bf6
#define _L4_BITFIELD7(type, bf1, bf2, bf3, bf4, bf5, bf6, bf7) \
  type bf1; type bf2; type bf3; type bf4; type bf5; type bf6; type bf7
#else
#if _L4_BYTE_ORDER == _L4_BIG_ENDIAN
#define _L4_BITFIELD2(type, bf1, bf2) type bf2; type bf1
#define _L4_BITFIELD3(type, bf1, bf2, bf3) type bf3; type bf2; type bf1
#define _L4_BITFIELD4(type, bf1, bf2, bf3, bf4) \
  type bf4; type bf3; type bf2; type bf1
#define _L4_BITFIELD5(type, bf1, bf2, bf3, bf4, bf5) \
  type bf5; type bf4; type bf3; type bf2; type bf1
#define _L4_BITFIELD6(type, bf1, bf2, bf3, bf4, bf5, bf6) \
  type bf6; type bf5; type bf4; type bf3; type bf2; type bf1
#define _L4_BITFIELD7(type, bf1, bf2, bf3, bf4, bf5, bf6, bf7) \
  type bf7; type bf6; type bf5; type bf4; type bf3; type bf2; type bf1
#else
#error "Unsupported endianess."
#endif
#endif


/* Use _L4_RAW to define a union of a raw accessor of type TYPE and a
   bit-field with one or more interpretations.  */
#define _L4_RAW(type, x)	\
  union				\
  {				\
    type raw;			\
    x;				\
  }

#define _L4_STRUCT1(s1)		struct s1
#define _L4_STRUCT2(s1, s2)	struct s1; struct s2
#define _L4_STRUCT3(s1, s2, s3)	struct s1; struct s2; struct s3
#define _L4_STRUCT4(s1, s2, s3, s4) \
  struct s1; struct s2; struct s3; struct s4


/* The following basic types need to be defined here, because they are
   liberally used in various header files (syscall.h for example).  */

typedef _L4_word_t _L4_api_version_t;
typedef _L4_word_t _L4_api_flags_t;
typedef _L4_word_t _L4_kernel_id_t;
struct _L4_kip;
typedef struct _L4_kip *_L4_kip_t;

typedef _L4_word_t _L4_thread_id_t;

/* The clock is 64 bits on all architectures.  The clock base is
   undefined.  The base unit is 1 microsecond.  */
typedef _L4_uint64_t _L4_clock_t;

typedef _L4_uint16_t _L4_time_t;

typedef _L4_word_t _L4_fpage_t;

typedef _L4_word_t _L4_msg_tag_t;


/* Now incorporate the public interfaces the user has selected.  */
#ifdef _L4_INTERFACE_L4
#include <l4/compat/types.h>
#endif
#ifdef _L4_INTERFACE_GNU
#include <l4/gnu/types.h>
#endif



#endif	/* l4/types.h */
