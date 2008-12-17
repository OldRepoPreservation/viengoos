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

#include <viengoos/cap.h>
#include <viengoos/folio.h>
#include <viengoos/thread.h>
#include <viengoos/messenger.h>
#include <hurd/as.h>
#include <hurd/stddef.h>
#include <assert.h>
#include <backtrace.h>

#ifdef RM_INTERN
#include <md5.h>

#include "../viengoos/cap.h"
#include "../viengoos/activity.h"
#endif

static void
print_nr (int width, int64_t nr, bool hex)
{
  int base = 10;
  if (hex)
    base = 16;

  int64_t v = nr;
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
	 struct vg_cap *root, vg_addr_t addr,
	 int indent, bool descend, const char *output_prefix)
{
  int i;

  struct vg_cap vg_cap = as_cap_lookup_rel (activity, root, addr, -1, NULL);
  if (vg_cap.type == vg_cap_void)
    return;

  if (! vg_cap_to_object (activity, &vg_cap))
    /* Cap is there but the object has been deallocated.  */
    return;

  if (output_prefix && *output_prefix)
    S_PRINTF ("%s: ", output_prefix);
  for (i = 0; i < indent; i ++)
    S_PRINTF (".");

  S_PRINTF ("[ ");
  if (index != -1)
    print_nr (3, index, false);
  else
    S_PRINTF ("root");
  S_PRINTF (" ] ");

  print_nr (12, vg_addr_prefix (addr), true);
  S_PRINTF ("/%d ", vg_addr_depth (addr));
  if (VG_CAP_GUARD_BITS (&vg_cap))
    S_PRINTF ("| 0x%llx/%d ", VG_CAP_GUARD (&vg_cap), VG_CAP_GUARD_BITS (&vg_cap));
  if (VG_CAP_SUBPAGES (&vg_cap) != 1)
    S_PRINTF ("(%d/%d) ", VG_CAP_SUBPAGE (&vg_cap), VG_CAP_SUBPAGES (&vg_cap));

  if (VG_CAP_GUARD_BITS (&vg_cap)
      && VG_ADDR_BITS - vg_addr_depth (addr) >= VG_CAP_GUARD_BITS (&vg_cap))
    S_PRINTF ("=> 0x%llx/%d ",
	    vg_addr_prefix (vg_addr_extend (addr,
				      VG_CAP_GUARD (&vg_cap),
				      VG_CAP_GUARD_BITS (&vg_cap))),
	    vg_addr_depth (addr) + VG_CAP_GUARD_BITS (&vg_cap));

#ifdef RM_INTERN
  S_PRINTF ("@" VG_OID_FMT " ", VG_OID_PRINTF (vg_cap.oid));
#endif
  S_PRINTF ("%s", vg_cap_type_string (vg_cap.type));

#ifdef RM_INTERN
  if (vg_cap.type == vg_cap_page || vg_cap.type == vg_cap_rpage)
    {
      struct object *object = cap_to_object_soft (root_activity, &vg_cap);
      if (object)
	{
	  struct md5_ctx ctx;
	  unsigned char result[16];

	  md5_init_ctx (&ctx);
	  md5_process_bytes (object, PAGESIZE, &ctx);
	  md5_finish_ctx (&ctx, result);

	  S_PRINTF (" ");
	  int i;
	  for (i = 0; i < 16; i ++)
	    printf ("%x%x", result[i] & 0xf, result[i] >> 4);

	  for (i = 0; i < PAGESIZE / sizeof (int); i ++)
	    if (((int *) (object))[i] != 0)
	      break;
	  if (i == PAGESIZE / sizeof (int))
	    S_PRINTF (" zero page");
	}
    }
#endif

  if (! descend)
    S_PRINTF ("...");

  S_PRINTF ("\n");

  if (! descend)
    return;

  if (vg_addr_depth (addr) + VG_CAP_GUARD_BITS (&vg_cap) > VG_ADDR_BITS)
    return;

  addr = vg_addr_extend (addr, VG_CAP_GUARD (&vg_cap), VG_CAP_GUARD_BITS (&vg_cap));

  switch (vg_cap.type)
    {
    case vg_cap_cappage:
    case vg_cap_rcappage:
      if (vg_addr_depth (addr) + VG_CAP_SUBPAGE_SIZE_LOG2 (&vg_cap) > VG_ADDR_BITS)
	return;

      for (i = 0; i < VG_CAP_SUBPAGE_SIZE (&vg_cap); i ++)
	do_walk (activity, i, root,
		 vg_addr_extend (addr, i, VG_CAP_SUBPAGE_SIZE_LOG2 (&vg_cap)),
		 indent + 1, true, output_prefix);

      return;

    case vg_cap_folio:
      if (vg_addr_depth (addr) + VG_FOLIO_OBJECTS_LOG2 > VG_ADDR_BITS)
	return;

      for (i = 0; i < VG_FOLIO_OBJECTS; i ++)
	do_walk (activity, i, root,
		 vg_addr_extend (addr, i, VG_FOLIO_OBJECTS_LOG2),
		 indent + 1, false, output_prefix);

      return;

    case vg_cap_thread:
      if (vg_addr_depth (addr) + VG_THREAD_SLOTS_LOG2 > VG_ADDR_BITS)
	return;

      for (i = 0; i < VG_THREAD_SLOTS; i ++)
	do_walk (activity, i, root,
		 vg_addr_extend (addr, i, VG_THREAD_SLOTS_LOG2),
		 indent + 1, true, output_prefix);

      return;

    case vg_cap_messenger:
      /* rmessenger's don't expose their capability slots.  */
      if (vg_addr_depth (addr) + VG_MESSENGER_SLOTS_LOG2 > VG_ADDR_BITS)
	return;

      for (i = 0; i < VG_MESSENGER_SLOTS; i ++)
	do_walk (activity, i, root,
		 vg_addr_extend (addr, i, VG_MESSENGER_SLOTS_LOG2),
		 indent + 1, true, output_prefix);

      return;

    default:
      return;
    }
}

/* AS_LOCK must not be held.  */
void
as_dump_from (activity_t activity, struct vg_cap *root, const char *prefix)
{
  debug (0, "Dumping address space.");
  backtrace_print ();

  if (0)
    do_walk (activity, -1, root, VG_ADDR (0, 0), 0, true, prefix);
}
