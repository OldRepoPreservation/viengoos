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

#ifndef _VIENGOOS_ADDR_H
#define _VIENGOOS_ADDR_H 1

#include <hurd/types.h>
#include <viengoos/math.h>
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
#define VG_ADDR_BITS 63
/* Client-side capability handle.  */
typedef struct addr vg_addr_t;

#define VG_ADDR_FMT "%llx/%d"
#define VG_ADDR_PRINTF(addr_) vg_addr_prefix ((addr_)), vg_addr_depth ((addr_))

/* Create an address given a prefix and a depth.  */
#define VG_ADDR(prefix_, depth_) \
  ({ \
    uint64_t p_ = (prefix_); \
    uint64_t d_ = (depth_); \
    assert (0 <= d_ && d_ <= VG_ADDR_BITS); \
    assert ((p_ & ((1 << (VG_ADDR_BITS - d_)) - 1)) == 0); \
    assert (p_ < (1ULL << VG_ADDR_BITS)); \
    (struct addr) { (p_ << 1ULL) | (1ULL << (VG_ADDR_BITS - d_)) }; \
  })

/* Create an address given a prefix and a depth.  Appropriate for use
   as an initializer.  */
#define VG_ADDR_INIT(prefix_, depth_) \
  { .raw = ((((prefix_) << 1) | 1) << (VG_ADDR_BITS - (depth_))) }

#define VG_ADDR_VOID ((struct addr) { 0ULL })
#define VG_ADDR_EQ(a, b) (a.raw == b.raw)
#define VG_ADDR_IS_VOID(a) (VG_ADDR_EQ (a, VG_ADDR_VOID))

/* Return ADDR_'s depth.  */
static inline int
vg_addr_depth (vg_addr_t addr)
{
  return VG_ADDR_BITS - (vg_lsb64 (addr.raw) - 1);
}

/* Return VG_ADDR's prefix.  */
static inline uint64_t
vg_addr_prefix (vg_addr_t addr)
{
  /* (Clear the boundary bit and shift right 1.)  */
  return (addr.raw & ~(1ULL << (VG_ADDR_BITS - vg_addr_depth (addr)))) >> 1;
}

/* Extend the address VG_ADDR by concatenating the lowest DEPTH bits of
   PREFIX.  */
#if 0
static inline vg_addr_t
vg_addr_extend (vg_addr_t addr, uint64_t prefix, int depth)
{
  assertx (depth >= 0, "depth: %d", depth);
  assertx (vg_addr_depth (addr) + depth <= VG_ADDR_BITS,
	   "addr: " VG_ADDR_FMT "; depth: %d", VG_ADDR_PRINTF (addr), depth);
  assertx (prefix < (1ULL << depth),
	   "prefix: %llx; depth: %lld", prefix, 1ULL << depth);
  return VG_ADDR (vg_addr_prefix (addr)
	       | (prefix << (VG_ADDR_BITS - vg_addr_depth (addr) - depth)),
	       vg_addr_depth (addr) + depth);
}
#else
#define vg_addr_extend(addr_, prefix_, depth_)				\
  ({									\
    vg_addr_t a__ = (addr_);						\
    uint64_t p__ = (prefix_);					\
    int d__ = (depth_);							\
    assertx (d__ >= 0, "depth: %d", d__);				\
    assertx (vg_addr_depth ((a__)) + (d__) <= VG_ADDR_BITS,			\
	     "addr: " VG_ADDR_FMT "; depth: %d", VG_ADDR_PRINTF (a__), d__);	\
    assertx (p__ < (1ULL << d__),					\
	     "prefix: %llx; depth: %lld", p__, 1ULL << d__);		\
    VG_ADDR (vg_addr_prefix ((a__))						\
	  | ((p__) << (VG_ADDR_BITS - vg_addr_depth ((a__)) - (d__))),	\
	  vg_addr_depth ((a__)) + (d__));					\
  })
#endif

/* Decrease the depth of VG_ADDR by DEPTH.  */
static inline vg_addr_t
vg_addr_chop (vg_addr_t addr, int depth)
{
  int d = vg_addr_depth (addr) - depth;
  assert (d >= 0);

  return VG_ADDR (vg_addr_prefix (addr) & ~((1ULL << (VG_ADDR_BITS - d)) - 1), d);
}

/* Return the last WIDTH bits of address's VG_ADDR prefix.  */
static inline uint64_t
vg_addr_extract (vg_addr_t addr, int width)
{
  assert (width <= vg_addr_depth (addr));

  return (vg_addr_prefix (addr) >> (VG_ADDR_BITS - vg_addr_depth (addr)))
    & ((1ULL << width) - 1);
}

/* Convert an address to a pointer.  The address must name an object
   mapped in the machine data instruction accessible part of the
   address space.  */
#define VG_ADDR_TO_PTR(addr_) \
  ({ \
    assert (vg_addr_prefix ((addr_)) < ((uintptr_t) -1)); \
    assert (vg_addr_depth ((addr_)) == VG_ADDR_BITS); \
    (void *) (uintptr_t) vg_addr_prefix ((addr_)); \
  })

/* Convert a pointer to an address.  */
#define VG_PTR_TO_ADDR(ptr_) \
  (VG_ADDR ((uintptr_t) (ptr_), VG_ADDR_BITS))

/* Return the address of the page that would contain pointer PTR_.  */
#define VG_PTR_TO_PAGE(ptr_) \
  vg_addr_chop (VG_ADDR ((uintptr_t) (ptr_), VG_ADDR_BITS), PAGESIZE_LOG2)

static inline vg_addr_t
vg_addr_add (vg_addr_t addr, uint64_t count)
{
  int w = VG_ADDR_BITS - vg_addr_depth (addr);

  return VG_ADDR (vg_addr_prefix (addr) + (count << w),
	       vg_addr_depth (addr));
}

static inline vg_addr_t
vg_addr_sub (vg_addr_t addr, uint64_t count)
{
  return vg_addr_add (addr, - count);
}

#endif
