/* types.h - Public interface for L4 compatibility types.
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
# error "Never use <l4/compat/types.h> directly; include <l4/types.h> instead."
#endif

typedef l4_word_t L4_Word_t;
typedef l4_uint64_t L4_Word64_t;
typedef l4_word_t L4_Bool_t;


/* Clock interface.  */

typedef _L4_RAW
(L4_Word64_t, _L4_STRUCT1
 ({
   l4_clock_t clock;
 })) L4_Clock_t;



#define _L4_CLOCK_OP(name, op)						\
static inline L4_Clock_t						\
L4_Clock ## name ## Usec (const L4_Clock_t clock, const L4_Word64_t usec) \
{									\
  L4_Clock_t new_clock;							\
  new_clock.clock = clock.clock op usec;				\
  return new_clock;							\
}

_L4_CLOCK_OP(Add, +)
_L4_CLOCK_OP(Sub, -)
#undef _L4_CLOCK_OP


#define _L4_CLOCK_OP(name, op)						\
static inline L4_Bool_t							\
L4_Clock ## name (const L4_Clock_t clock1, const L4_Clock_t clock2)	\
{									\
  return clock1.clock op clock2.clock;					\
}

_L4_CLOCK_OP(Earlier, <)
_L4_CLOCK_OP(Later, >)
_L4_CLOCK_OP(Equal, ==)
_L4_CLOCK_OP(NotEqual, !=)
#undef _L4_CLOCK_OP


#if defined(__cplusplus)

#define _L4_CLOCK_OP(op, type)						\
static inline L4_Clock_t						\
operator ## op ## (const L4_Clock_t& clock, const type usec)		\
{									\
  L4_Clock_t new_clock;							\
  new_clock.clock = clock op usec;					\
  return new_clock;							\
}

_L4_CLOCK_OP(+, int)
_L4_CLOCK_OP(+, L4_Word64_t)
_L4_CLOCK_OP(-, int)
_L4_CLOCK_OP(-, L4_Word64_t)
#undef _L4_CLOCK_OP


#define _L4_CLOCK_OP(op)						\
static inline L4_Bool_t							\
operator ## op ## (const L4_Clock_t& clock1, const L4_Clock_t& clock2)	\
{									\
  return clock1.clock op clock2.clock;					\
}

_L4_CLOCK_OP(<)
_L4_CLOCK_OP(>)
_L4_CLOCK_OP(<=)
_L4_CLOCK_OP(>=)
_L4_CLOCK_OP(==)
_L4_CLOCK_OP(!=)
#undef _L4_CLOCK_OP

#endif /* __cplusplus */
