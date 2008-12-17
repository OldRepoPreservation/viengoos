/* addr-trans.h - Address translation functions.
   Copyright (C) 2007, 2008 Free Software Foundation, Inc.
   Written by Neal H. Walfield <neal@gnu.org>.

   This file is part of the GNU Hurd.

   GNU Hurd is free software: you can redistribute it and/or modify it
   under the terms of the GNU Lesser General Public License as
   published by the Free Software Foundation, either version 3 of the
   License, or (at your option) any later version.

   GNU Hurd is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with GNU Hurd.  If not, see
   <http://www.gnu.org/licenses/>.  */

#ifndef _VIENGOOS_ADDR_TRANS_H
#define _VIENGOOS_ADDR_TRANS_H

#include <stdint.h>
#include <hurd/stddef.h>
#include <hurd/math.h>

/* Capabilities have two primary functions: they designate objects and
   they participate in address translation.  This structure controls
   how the page table walker translates bits when passing through this
   capability.  */

#define CAP_ADDR_TRANS_GUARD_SUBPAGE_BITS 22
#define CAP_ADDR_TRANS_SUBPAGES_BITS 4
#define CAP_ADDR_TRANS_GDEPTH_BITS 6

struct cap_addr_trans
{
  union
  {
    struct
    {
      /* The value of the guard and the subpage to use.

      A capability page is partitioned into 2^SUBPAGES_LOG2 subpages.
      This value determines the number of subpage index bits and
      maximum number of guard bits.  The number of subpage index bits
      is SUBPAGES_LOG2 and the number of guard bits is the remainder
      (the guard lies in the upper bits; the subpage in the lower).

      If SUBPAGES_LOG2 is 0, there is a single subpage (covering the
      entire page).  This implies that there are no subpage bits (the
      only valid offset is 0) and 21 possible guard bits.  If
      SUBPAGES_LOG2 is 0, there are 256 subpages, 8 subpage bits and a
      maximum of 21-8=15 guard bits.  */
      uint32_t guard_subpage: CAP_ADDR_TRANS_GUARD_SUBPAGE_BITS;
      /* The log2 of the subpages.  The size of a subpage is thus 2^(8 -
	 SUBPAGES_LOG2).  Values of SUBPAGES_LOG2 other than 0 are only
	 allowed for cap pages.  */
      uint32_t subpages_log2: CAP_ADDR_TRANS_SUBPAGES_BITS;
      /* Number of significant guard bits.  The value of the GUARD is zero
	 extended if GDEPTH is greater than the number of available guard
	 bits.  */
      uint32_t gdepth: CAP_ADDR_TRANS_GDEPTH_BITS;
    };
    uint32_t raw;
  };
};

#define CAP_ADDR_TRANS_INIT { { .raw = 0 } }
#define CAP_ADDR_TRANS_VOID (struct cap_addr_trans) { { .raw = 0 } }

/* The log2 number of subpages.  */
#define CAP_ADDR_TRANS_SUBPAGES_LOG2(cap_addr_trans_) \
  ((cap_addr_trans_).subpages_log2)

/* The number of subpages.  */
#define CAP_ADDR_TRANS_SUBPAGES(cap_addr_trans_) \
  (1 << CAP_ADDR_TRANS_SUBPAGES_LOG2((cap_addr_trans_)))

/* The designated subpage.  */
#define CAP_ADDR_TRANS_SUBPAGE(cap_addr_trans_) \
  ((cap_addr_trans_).guard_subpage \
   & (CAP_ADDR_TRANS_SUBPAGES ((cap_addr_trans_)) - 1))

/* The log2 of the size of the named subpage (in capability
   units).  */
#define CAP_ADDR_TRANS_SUBPAGE_SIZE_LOG2(cap_addr_trans_) \
  (8 - (cap_addr_trans_).subpages_log2)

/* The number of caps addressed by this capability.  */
#define CAP_ADDR_TRANS_SUBPAGE_SIZE(cap_addr_trans_) \
  (1 << CAP_ADDR_TRANS_SUBPAGE_SIZE_LOG2 ((cap_addr_trans_)))

/* The offset in capability units (with respect to the start of the
   capability page) of the first capability in the designated
   sub-page.  */
#define CAP_ADDR_TRANS_SUBPAGE_OFFSET(cap_addr_trans_) \
  (CAP_ADDR_TRANS_SUBPAGE ((cap_addr_trans_)) \
   * CAP_ADDR_TRANS_SUBPAGE_SIZE ((cap_addr_trans_)))

/* The number of guard bits.  */
#define CAP_ADDR_TRANS_GUARD_BITS(cap_addr_trans_) ((cap_addr_trans_).gdepth)

/* The value of the guard.  */
#define CAP_ADDR_TRANS_GUARD(cap_addr_trans_) \
  ((uint64_t) ((cap_addr_trans_).guard_subpage \
	       >> (cap_addr_trans_).subpages_log2))

#define CATSGST_(test_, format, args...) \
  if (! (test_)) \
    { \
      r_ = false; \
      debug (1, format, ##args); \
    }

/* Set CAP_ADDR_TRANS_P_'s guard and the subpage.  Returns true on success
   (parameters valid), false otherwise.  */
#define CAP_ADDR_TRANS_SET_GUARD_SUBPAGE(cap_addr_trans_p_, guard_, gdepth_, \
					 subpage_, subpages_) \
  ({ bool r_ = true; \
     /* There must be at least 1 subpage.  */ \
     CATSGST_ (((subpages_) > 0), \
	       "subpages_ (%d) must be at least 1\n", (subpages_)); \
     CATSGST_ (((subpages_) & ((subpages_) - 1)) == 0, \
               "SUBPAGES_ (%d) must be a power of 2\n", (subpages_)); \
     int subpages_log2_ = vg_msb ((subpages_)) - 1; \
     CATSGST_ (subpages_log2_ <= 8, \
               "maximum subpages is 256 (%d)\n", (subpages_)); \
     CATSGST_ (0 <= (subpage_) && (subpage_) < (subpages_), \
               "subpage (%d) must be between 0 and SUBPAGES_ (%d) - 1\n", \
               (subpage_), (subpages_)); \
     \
     /* The number of required guard bits.  */ \
     int gbits_ = vg_msb64 ((guard_)); \
     CATSGST_ (gbits_ <= (gdepth_), \
               "Significant guard bits (%d) must be less than depth (%d)\n", \
               gbits_, (gdepth_)); \
     CATSGST_ (gbits_ + subpages_log2_ <= CAP_ADDR_TRANS_GUARD_SUBPAGE_BITS, \
               "Significant guard bits (%d) plus subpage bits (%d) > %d\n", \
               gbits_, subpages_log2_, CAP_ADDR_TRANS_GUARD_SUBPAGE_BITS); \
     \
     if (r_) \
       { \
         (cap_addr_trans_p_)->subpages_log2 = subpages_log2_; \
         (cap_addr_trans_p_)->gdepth = (gdepth_); \
         (cap_addr_trans_p_)->guard_subpage \
           = ((guard_) << subpages_log2_) | (subpage_); \
       } \
     r_; \
  })

/* Set *CAP_ADDR_TRANS_P_'s guard.  Returns true on success (parameters
   valid), false otherwise.  */
#define CAP_ADDR_TRANS_SET_GUARD(cap_addr_trans_p_, guard_, gdepth_) \
  ({ int subpage_ = CAP_ADDR_TRANS_SUBPAGE (*(cap_addr_trans_p_)); \
     int subpages_ = CAP_ADDR_TRANS_SUBPAGES (*(cap_addr_trans_p_)); \
     CAP_ADDR_TRANS_SET_GUARD_SUBPAGE ((cap_addr_trans_p_), \
				       (guard_), (gdepth_), \
				       (subpage_), (subpages_)); \
  })

/* Set *CAP_ADDR_TRANS_P_'s subpage.  Returns true on success (parameters
   valid), false otherwise.  */
#define CAP_ADDR_TRANS_SET_SUBPAGE(cap_addr_trans_p_, subpage_, subpages_) \
  ({ int gdepth_ = CAP_ADDR_TRANS_GUARD_BITS (*(cap_addr_trans_p_)); \
     int guard_ = CAP_ADDR_TRANS_GUARD (*(cap_addr_trans_p_)); \
     CAP_ADDR_TRANS_SET_GUARD_SUBPAGE ((cap_addr_trans_p_), \
				       (guard_), (gdepth_), \
				       (subpage_), (subpages_)); \
  })

/* Returns whether the capability address CAP_ADDR_TRANS is well-formed.  */
#define CAP_ADDR_TRANS_VALID(cap_addr_trans) \
  ({ bool r_ = true; \
     CATSGST_ (CAP_ADDR_TRANS_GUARD_BITS (cap_addr_trans) <= WORDSIZE, \
	       "Invalid guard depth (%d)", \
	       CAP_ADDR_TRANS_GUARD_BITS (cap_addr_trans)); \
     CATSGST_ (CAP_ADDR_TRANS_SUBPAGES_LOG2 (cap_addr_trans) <= 8, \
               "Invalid number of subpages (%d)", \
               CAP_ADDR_TRANS_SUBPAGES (cap_addr_trans)); \
     CATSGST_ (vg_msb (CAP_ADDR_TRANS_GUARD (cap_addr_trans)) \
	       <= CAP_ADDR_TRANS_GUARD_BITS (cap_addr_trans), \
               "Significant guard bits (%d) exceeds guard depth (%d)", \
               vg_msb (CAP_ADDR_TRANS_GUARD (cap_addr_trans)), \
	       CAP_ADDR_TRANS_GUARD_BITS (cap_addr_trans)); \
     r_; \
  })

#endif
