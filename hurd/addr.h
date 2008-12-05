/* addr.h - Address definitions.
   Copyright (C) 2007, 2008 Free Software Foundation, Inc.
   Written by Neal H. Walfield <neal@gnu.org>.

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

#ifndef _HURD_ADDR_H
#define _HURD_ADDR_H 1

#include <hurd/types.h>
#include <hurd/math.h>
#include <stdint.h>

#include <assert.h>

/* Addresses are 64-bits wide and translate up to 63 bits.  They are
   composed of a depth and a prefix that is depth bits wide.

   The 64-bit field is packed as follows: the upper bits are encoded
   in binary and represent the prefix, these are followed by a single
   bit that is on, which is followed by a number encoded in unary.
   The value of the unary number is 63 - depth.  This allows easy
   calculation of the depth and extraction of the prefix.  Thus,
   given:

      xxxxx100

   The unary value is 2 yielding a depth of 63 - 2 = 61.  These bits
   are encoded in the upper DEPTH bits of the field.

   Leaves thus have a 1 in the least significant bit and nodes a
   0.  */
struct addr
{
  uint64_t raw;
};
#define ADDR_BITS 63
/* Client-side capability handle.  */
typedef struct addr addr_t;

#define ADDR_FMT "%llx/%d"
#define ADDR_PRINTF(addr_) addr_prefix ((addr_)), addr_depth ((addr_))

/* Create an address given a prefix and a depth.  */
#define ADDR(prefix_, depth_) \
  ({ \
    uint64_t p_ = (prefix_); \
    uint64_t d_ = (depth_); \
    assert (0 <= d_ && d_ <= ADDR_BITS); \
    assert ((p_ & ((1 << (ADDR_BITS - d_)) - 1)) == 0); \
    assert (p_ < (1ULL << ADDR_BITS)); \
    (struct addr) { (p_ << 1ULL) | (1ULL << (ADDR_BITS - d_)) }; \
  })

/* Create an address given a prefix and a depth.  Appropriate for use
   as an initializer.  */
#define ADDR_INIT(prefix_, depth_) \
  { .raw = ((((prefix_) << 1) | 1) << (ADDR_BITS - (depth_))) }

#define ADDR_VOID ((struct addr) { 0ULL })
#define ADDR_EQ(a, b) (a.raw == b.raw)
#define ADDR_IS_VOID(a) (ADDR_EQ (a, ADDR_VOID))

/* Return ADDR_'s depth.  */
static inline int
addr_depth (addr_t addr)
{
  return ADDR_BITS - (vg_lsb64 (addr.raw) - 1);
}

/* Return ADDR's prefix.  */
static inline uint64_t
addr_prefix (addr_t addr)
{
  /* (Clear the boundary bit and shift right 1.)  */
  return (addr.raw & ~(1ULL << (ADDR_BITS - addr_depth (addr)))) >> 1;
}

/* Extend the address ADDR by concatenating the lowest DEPTH bits of
   PREFIX.  */
#if 0
static inline addr_t
addr_extend (addr_t addr, uint64_t prefix, int depth)
{
  assertx (depth >= 0, "depth: %d", depth);
  assertx (addr_depth (addr) + depth <= ADDR_BITS,
	   "addr: " ADDR_FMT "; depth: %d", ADDR_PRINTF (addr), depth);
  assertx (prefix < (1ULL << depth),
	   "prefix: %llx; depth: %lld", prefix, 1ULL << depth);
  return ADDR (addr_prefix (addr)
	       | (prefix << (ADDR_BITS - addr_depth (addr) - depth)),
	       addr_depth (addr) + depth);
}
#else
#define addr_extend(addr_, prefix_, depth_)				\
  ({									\
    addr_t a__ = (addr_);						\
    uint64_t p__ = (prefix_);					\
    int d__ = (depth_);							\
    assertx (d__ >= 0, "depth: %d", d__);				\
    assertx (addr_depth ((a__)) + (d__) <= ADDR_BITS,			\
	     "addr: " ADDR_FMT "; depth: %d", ADDR_PRINTF (a__), d__);	\
    assertx (p__ < (1ULL << d__),					\
	     "prefix: %llx; depth: %lld", p__, 1ULL << d__);		\
    ADDR (addr_prefix ((a__))						\
	  | ((p__) << (ADDR_BITS - addr_depth ((a__)) - (d__))),	\
	  addr_depth ((a__)) + (d__));					\
  })
#endif

/* Decrease the depth of ADDR by DEPTH.  */
static inline addr_t
addr_chop (addr_t addr, int depth)
{
  int d = addr_depth (addr) - depth;
  assert (d >= 0);

  return ADDR (addr_prefix (addr) & ~((1ULL << (ADDR_BITS - d)) - 1), d);
}

/* Return the last WIDTH bits of address's ADDR prefix.  */
static inline uint64_t
addr_extract (addr_t addr, int width)
{
  assert (width <= addr_depth (addr));

  return (addr_prefix (addr) >> (ADDR_BITS - addr_depth (addr)))
    & ((1ULL << width) - 1);
}

/* Convert an address to a pointer.  The address must name an object
   mapped in the machine data instruction accessible part of the
   address space.  */
#define ADDR_TO_PTR(addr_) \
  ({ \
    assert (addr_prefix ((addr_)) < ((uintptr_t) -1)); \
    assert (addr_depth ((addr_)) == ADDR_BITS); \
    (void *) (uintptr_t) addr_prefix ((addr_)); \
  })

/* Convert a pointer to an address.  */
#define PTR_TO_ADDR(ptr_) \
  (ADDR ((uintptr_t) (ptr_), ADDR_BITS))

static inline addr_t
addr_add (addr_t addr, uint64_t count)
{
  int w = ADDR_BITS - addr_depth (addr);

  return ADDR (addr_prefix (addr) + (count << w),
	       addr_depth (addr));
}

static inline addr_t
addr_sub (addr_t addr, uint64_t count)
{
  return addr_add (addr, - count);
}

#endif
