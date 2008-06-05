/* as-dump.c - Address space dumper.
   Copyright (C) 2007, 2008 Free Software Foundation, Inc.
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

#include <hurd/cap.h>
#include <hurd/folio.h>
#include <hurd/as.h>
#include <hurd/stddef.h>
#include <assert.h>

static void
print_nr (int width, l4_int64_t nr, bool hex)
{
  int base = 10;
  if (hex)
    base = 16;

  l4_int64_t v = nr;
  int w = 0;
  if (v < 0)
    {
      v = -v;
      w ++;
    }
  do
    {
      w ++;
      v /= base;
    }
  while (v > 0);

  int i;
  for (i = w; i < width; i ++)
    S_PUTCHAR (' ');

  if (hex)
    S_PRINTF ("0x%llx", nr);
  else
    S_PRINTF ("%lld", nr);
}

static void
do_walk (activity_t activity, int index,
	 struct cap *root, addr_t addr,
	 int indent, bool descend, const char *output_prefix)
{
  int i;

  struct cap cap = as_cap_lookup_rel (activity, root, addr, -1, NULL);
  if (cap.type == cap_void)
    return;

  if (! cap_to_object (activity, &cap))
    /* Cap is there but the object has been deallocated.  */
    return;

  if (output_prefix)
    S_PRINTF ("%s: ", output_prefix);
  for (i = 0; i < indent; i ++)
    S_PRINTF (".");

  S_PRINTF ("[ ");
  if (index != -1)
    print_nr (3, index, false);
  else
    S_PRINTF ("root");
  S_PRINTF (" ] ");

  print_nr (12, addr_prefix (addr), true);
  S_PRINTF ("/%d ", addr_depth (addr));
  if (CAP_GUARD_BITS (&cap))
    S_PRINTF ("| 0x%llx/%d ", CAP_GUARD (&cap), CAP_GUARD_BITS (&cap));
  if (CAP_SUBPAGES (&cap) != 1)
    S_PRINTF ("(%d/%d) ", CAP_SUBPAGE (&cap), CAP_SUBPAGES (&cap));

  if (CAP_GUARD_BITS (&cap)
      && ADDR_BITS - addr_depth (addr) >= CAP_GUARD_BITS (&cap))
    S_PRINTF ("=> 0x%llx/%d ",
	    addr_prefix (addr_extend (addr,
				      CAP_GUARD (&cap),
				      CAP_GUARD_BITS (&cap))),
	    addr_depth (addr) + CAP_GUARD_BITS (&cap));

#ifdef RM_INTERN
  S_PRINTF ("@" OID_FMT " ", OID_PRINTF (cap.oid));
#endif
  S_PRINTF ("%s", cap_type_string (cap.type));

  if (! descend)
    S_PRINTF ("...");

  S_PRINTF ("\n");

  if (! descend)
    return;

  if (addr_depth (addr) + CAP_GUARD_BITS (&cap) > ADDR_BITS)
    return;

  addr = addr_extend (addr, CAP_GUARD (&cap), CAP_GUARD_BITS (&cap));

  switch (cap.type)
    {
    case cap_cappage:
    case cap_rcappage:
      if (addr_depth (addr) + CAP_SUBPAGE_SIZE_LOG2 (&cap) > ADDR_BITS)
	return;

      for (i = 0; i < CAP_SUBPAGE_SIZE (&cap); i ++)
	do_walk (activity, i, root,
		 addr_extend (addr, i, CAP_SUBPAGE_SIZE_LOG2 (&cap)),
		 indent + 1, true, output_prefix);

      return;

    case cap_folio:
      if (addr_depth (addr) + FOLIO_OBJECTS_LOG2 > ADDR_BITS)
	return;

      for (i = 0; i < FOLIO_OBJECTS; i ++)
	do_walk (activity, i, root,
		 addr_extend (addr, i, FOLIO_OBJECTS_LOG2),
		 indent + 1, false, output_prefix);

      return;

    default:
      return;
    }
}

/* AS_LOCK must not be held.  */
void
as_dump_from (activity_t activity, struct cap *root, const char *prefix)
{
  // do_walk (activity, -1, root, ADDR (0, 0), 0, true, prefix);
}
