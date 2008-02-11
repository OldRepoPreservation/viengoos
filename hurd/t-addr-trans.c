/* t-cap.c - Test the implementation of the various cap functions.
   Copyright (C) 2007 Free Software Foundation, Inc.
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

#include <assert.h>
#include <stdbool.h>
#include <l4/math.h>
#include <l4/types.h>

#include "stddef.h"
#include "addr-trans.h"

int output_debug;
char *program_name = "t-addr-trans";

int
main (int argc, char *argv[])
{
  printf ("Checking CAP_ADDR_TRANS_SET_GUARD_SUBPAGE... ");

  struct cap_addr_trans cap_addr_trans;

  bool r;
  int subpage_bits;
  for (subpage_bits = 0; subpage_bits < 16; subpage_bits ++)
    {
      int subpages = 1 << subpage_bits;
      int subpage_size_log2 = 8 - subpage_bits;
      int subpage_size = 1 << subpage_size_log2;

      memset (&cap_addr_trans, 0, sizeof (cap_addr_trans));

      r = CAP_ADDR_TRANS_SET_SUBPAGE (&cap_addr_trans, 0, subpages);
      assert (r == (subpage_bits <= 8));
      if (subpage_bits >= 8)
	continue;

      assert (CAP_ADDR_TRANS_SUBPAGES (cap_addr_trans) == subpages);
      assert (CAP_ADDR_TRANS_SUBPAGE_SIZE (cap_addr_trans) == subpage_size);
      assert (CAP_ADDR_TRANS_SUBPAGE_SIZE_LOG2 (cap_addr_trans)
	      == subpage_size_log2);

      int gdepth;
      for (gdepth = 0; gdepth < L4_WORDSIZE; gdepth ++)
	{
	  int guard_bits;
	  for (guard_bits = 0; guard_bits < L4_WORDSIZE; guard_bits ++)
	    {
	      int guard = (1 << guard_bits) - 1;
	      r = CAP_ADDR_TRANS_SET_GUARD (&cap_addr_trans, guard, gdepth);
	      if (guard_bits <= gdepth
		  && (guard_bits + subpage_bits
		      <= CAP_ADDR_TRANS_GUARD_SUBPAGE_BITS))
		{
		  assert (r);
		  assert (CAP_ADDR_TRANS_GUARD_BITS (cap_addr_trans)
			  == gdepth);
		  assert (CAP_ADDR_TRANS_GUARD (cap_addr_trans) == guard);
		}
	      else
		assert (! r);
	    }
	}
    }

  printf ("ok\n");

  return 0;
}
