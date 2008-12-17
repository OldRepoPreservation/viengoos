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
#include <viengoos/addr.h>
#include <assert.h>
#include <viengoos/math.h>

char *program_name = "t-addr";
int output_debug = 0;

int
main (int argc, char *argv[])
{
  vg_addr_t addr;
  int i, j;

  printf ("Checking VG_ADDR... ");
  for (i = 0; i < VG_ADDR_BITS; i ++)
    {
      addr = VG_ADDR (1ULL << i, VG_ADDR_BITS - i);
      debug (1, "%llx/%d =? %llx/%d\n",
	     1ULL << i, VG_ADDR_BITS - i,
	     vg_addr_prefix (addr), vg_addr_depth (addr));
      assert (vg_addr_depth (addr) == VG_ADDR_BITS - i);
      assert (vg_addr_prefix (addr) == 1ull << i);
    }
  printf ("ok.\n");

  printf ("Checking vg_addr_extend... ");
  addr = VG_ADDR (0, 0);
  for (i = 1; i < VG_ADDR_BITS; i ++)
    {
      addr = vg_addr_extend (addr, 1, 1);
      assert (vg_addr_depth (addr) == i);
      assert (vg_msb64 (vg_addr_prefix (addr)) == VG_ADDR_BITS);
      assert (vg_lsb64 (vg_addr_prefix (addr)) == VG_ADDR_BITS - i + 1);
    }
  printf ("ok.\n");

  printf ("Checking vg_addr_extract... ");
  addr = VG_ADDR (0, 0);
  for (i = 0; i < VG_ADDR_BITS; i ++)
    {
      addr = VG_ADDR (((1ULL << i) - 1) << (VG_ADDR_BITS - i), i);

      for (j = 0; j <= i; j ++)
	{
	  l4_uint64_t idx = vg_addr_extract (addr, j);
	  assert (idx == (1ULL << j) - 1);
	}
    }
  printf ("ok.\n");

  return 0;
}
