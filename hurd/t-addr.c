/* t-addr.c - Test the implementation of the various addr functions.
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

#include <hurd/stddef.h>
#include <hurd/types.h>
#include <hurd/addr.h>
#include <assert.h>
#include <l4/math.h>

const char program_name[] = "t-addr";
int output_debug = 0;

int
main (int argc, char *argv[])
{
  addr_t addr;
  int i, j;

  printf ("Checking ADDR... ");
  for (i = 0; i < ADDR_BITS; i ++)
    {
      addr = ADDR (1ULL << i, ADDR_BITS - i);
      debug (1, "%llx/%d =? %llx/%d\n",
	     1ULL << i, ADDR_BITS - i,
	     addr_prefix (addr), addr_depth (addr));
      assert (addr_depth (addr) == ADDR_BITS - i);
      assert (addr_prefix (addr) == 1ull << i);
    }
  printf ("ok.\n");

  printf ("Checking addr_extend... ");
  addr = ADDR (0, 0);
  for (i = 1; i < ADDR_BITS; i ++)
    {
      addr = addr_extend (addr, 1, 1);
      assert (addr_depth (addr) == i);
      assert (l4_msb64 (addr_prefix (addr)) == ADDR_BITS);
      assert (l4_lsb64 (addr_prefix (addr)) == ADDR_BITS - i + 1);
    }
  printf ("ok.\n");

  printf ("Checking addr_extract... ");
  addr = ADDR (0, 0);
  for (i = 0; i < ADDR_BITS; i ++)
    {
      addr = ADDR (((1ULL << i) - 1) << (ADDR_BITS - i), i);

      for (j = 0; j <= i; j ++)
	{
	  l4_uint64_t idx = addr_extract (addr, j);
	  assert (idx == (1ULL << j) - 1);
	}
    }
  printf ("ok.\n");

  return 0;
}
