/* types.h - Public interface for L4 types.
   Copyright (C) 2003 Free Software Foundation, Inc.
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

#define L4_LITTLE_ENDIAN	0
#define L4_BIG_ENDIAN		1

#define L4_WORDSIZE_32		0
#define L4_WORDSIZE_64		1

/* <l4/bits/types.h> defines L4_BYTE_ORDER and L4_WORDSIZE.  It also
   defines the basic type l4_word_t.  */
#include <l4/bits/types.h>

# define __l4_intN_t(N, MODE) \
  typedef int l4_int##N##_t __attribute__ ((__mode__ (MODE)))
# define __l4_uintN_t(N, MODE) \
  typedef unsigned int l4_uint##N##_t __attribute__ ((__mode__ (MODE)))

__l4_intN_t (8, __QI__);
__l4_intN_t (16, __HI__);
__l4_intN_t (32, __SI__);
__l4_intN_t (64, __DI__);

__l4_uintN_t (8, __QI__);
__l4_uintN_t (16, __HI__);
__l4_uintN_t (32, __SI__);
__l4_uintN_t (64, __DI__);

#if L4_WORDSIZE == L4_WORDSIZE_32
typedef l4_uint32_t l4_word_t;
#else
#if L4_WORDSIZE == L4_WORDSIZE_64
typedef l4_uint64_t l4_word_t;
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
#if L4_WORDSIZE == L4_WORDSIZE_32
#define _L4_BITFIELD_64(name, nr) : 0
#define _L4_BITFIELD_32_64(name, nr32, nr64) name : nr32
#else
#if L4_WORDSIZE == L4_WORDSIZE_64
#define _L4_BITFIELD_64(name, nr) name : nr
#define _L4_BITFIELD_32_64(name, nr32, nr64) name : nr64
#endif
#endif

/* The representation of a bit-field is system and/or compiler
   specific.  We require that GCC uses a specific representation that
   only depends on the endianess of the system.  */
#if L4_BYTE_ORDER == L4_LITTLE_ENDIAN
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
#if L4_BYTE_ORDER == L4_BIG_ENDIAN
#define _L4_BITFIELD2(type, bf1, bf2) type bf2; type bf1
#define _L4_BITFIELD3(type, bf1, bf2, bf3) type bf3; type bf2; type bf1
#define _L4_BITFIELD4(type, bf1, bf2, bf3, bf4) \
  type bf4; type bf3; type bf2 type; type bf1
#define _L4_BITFIELD5(type, bf1, bf2, bf3, bf4, bf5) \
  type bf5; type bf4; type bf3; type bf2 type; type bf1
#define _L4_BITFIELD6(type, bf1, bf2, bf3, bf4, bf5, bf6) \
  type bf6; type bf5; type bf4; type bf3; type bf2; type bf1
#define _L4_BITFIELD6(type, bf1, bf2, bf3, bf4, bf5, bf6, bf7) \
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


/* The Thread ID type is here to avoid vregs.h requiring thread.h.  */
/* FIXME: Remove named structs when gcc supports initializers for
   unnamed fields, and change the initializers in <l4/thread.h>.  */
typedef _L4_RAW
(l4_word_t, _L4_STRUCT4
 ({
   _L4_BITFIELD2
     (l4_word_t,
      _L4_BITFIELD_32_64 (version, 14, 32),
      _L4_BITFIELD_32_64 (thread_no, 18, 32));
 },
 {
   _L4_BITFIELD2
     (l4_word_t,
      _L4_BITFIELD (_all_zero, 6),
      _L4_BITFIELD_32_64 (local, 26, 58));
 },
 {
   _L4_BITFIELD2
     (l4_word_t,
      _L4_BITFIELD_32_64 (version, 14, 32),
      _L4_BITFIELD_32_64 (thread_no, 18, 32));
 } global,
 {
   _L4_BITFIELD2
     (l4_word_t,
      _L4_BITFIELD (_all_zero, 6),
      _L4_BITFIELD_32_64 (local, 26, 58));
 } local)) l4_thread_id_t;



/* The clock is 64 bits on all architectures.  The clock base is
   undefined.  The base unit is 1 microsecond.  */
typedef l4_uint64_t l4_clock_t;


typedef _L4_RAW
(l4_uint16_t, _L4_STRUCT2
 ({
   /* This is a time period.  It is 2^e * m usec long.  */
   _L4_BITFIELD3
     (l4_uint16_t,
      _L4_BITFIELD (m, 10),
      _L4_BITFIELD (e, 5),
      _L4_BITFIELD (_zero, 1));
 } period,
 {
   /* This is a time point with validity (2^10 - 1) * 2^e.  */
   _L4_BITFIELD4
     (l4_uint16_t,
      _L4_BITFIELD (m, 10),
      _L4_BITFIELD (c, 1),
      _L4_BITFIELD (e, 4),
      _L4_BITFIELD (_one, 1));
 } point)) l4_time_t;


/* FIXME: Remove named structs when gcc supports initializers for
   unnamed fields, and change the initializers in <l4/space.h>.  */
typedef _L4_RAW
(l4_word_t, _L4_STRUCT4
 ({
   _L4_BITFIELD3
     (l4_word_t,
      _L4_BITFIELD (rights, 4),
      _L4_BITFIELD (log2_size, 6),
      _L4_BITFIELD_32_64 (base, 22, 54));
 },
 {
   /* Alias names for RIGHTS.  */
   _L4_BITFIELD3
     (l4_word_t,
      _L4_BITFIELD (executable, 1),
      _L4_BITFIELD (writable, 1),
      _L4_BITFIELD (readable, 1));
 },
 {
   /* Names for status bits as returned from l4_unmap.  */
   _L4_BITFIELD3
     (l4_word_t,
      _L4_BITFIELD (executed, 1),
      _L4_BITFIELD (written, 1),
      _L4_BITFIELD (referenced, 1));
 },
 {
   _L4_BITFIELD3
     (l4_word_t,
      _L4_BITFIELD (rights, 4),
      _L4_BITFIELD (log2_size, 6),
      _L4_BITFIELD_32_64 (base, 22, 54));
 } page)) l4_fpage_t;


/* Message tags.  */
typedef _L4_RAW (l4_word_t, _L4_STRUCT1 ({
  _L4_BITFIELD7
    (l4_word_t,
     _L4_BITFIELD (untyped, 6),
     _L4_BITFIELD (typed, 6),
     _L4_BITFIELD (propagated, 1),
     _L4_BITFIELD (redirected, 1),
     _L4_BITFIELD (cross_cpu, 1),
     _L4_BITFIELD (error, 1),
     _L4_BITFIELD_32_64 (label, 16, 48));
})) l4_msg_tag_t;


#ifndef _L4_NO_COMPAT

#include <l4/compat/types.h>

#endif	/* !_L4_NO_COMPAT */

#endif	/* l4/types.h */
