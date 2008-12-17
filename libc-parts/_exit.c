/* _exit.c - Exit implementation.
   Copyright (C) 2008 Free Software Foundation, Inc.
   Written by Neal H. Walfield <neal@gnu.org>.

   This file is part of the GNU Hurd.

   The GNU Hurd is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2, or (at
   your option) any later version.

   The GNU Hurd is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA. */

#include <hurd/stddef.h>
#include <hurd/startup.h>
#include <viengoos/folio.h>
#include <backtrace.h>

int __global_zero;

void
_exit (int ret)
{
#if defined (RM_INTERN)
# if defined (__i386__)
  /* Try to invoke the debugger.  */
  asm ("int $3");
# endif
#else
  backtrace_print ();

  extern struct hurd_startup_data *__hurd_startup_data;

  /* We try to kill the activity and, if that fails, the main
     thread.  */
  vg_addr_t objs[] = { __hurd_startup_data->activity,
		    __hurd_startup_data->thread };

  int o;
  for (o = 0; o < sizeof (objs) / sizeof (objs[0]); o ++)
    {
      int i;
      for (i = 0; i < __hurd_startup_data->desc_count; i ++)
	{
	  struct hurd_object_desc *desc = &__hurd_startup_data->descs[i];
	  if (VG_ADDR_EQ (desc->object, objs[o]))
	    {
	      if (VG_ADDR_IS_VOID (desc->storage))
		/* We don't own the storage and thus can't deallocate
		   the object.  */
		continue;

	      vg_addr_t folio = vg_addr_chop (desc->storage, VG_FOLIO_OBJECTS_LOG2);
	      int index = vg_addr_extract (desc->storage, VG_FOLIO_OBJECTS_LOG2);

	      error_t err;
	      err = rm_folio_object_alloc (VG_ADDR_VOID, folio, index,
					   vg_cap_void, VG_OBJECT_POLICY_VOID,
					   (uintptr_t) ret,
					   NULL, NULL);
	      if (err)
		debug (0, "deallocating object: %d", err);
	    }
	}
    }
#endif

  debug (0, "Failed to die gracefully; doing the ultra-violent.");

  volatile int j = ret / __global_zero;
  for (;;)
    {
      j --;
#ifndef RM_INTERN
      /* XXX: This doesn't work for laden.  */
      l4_yield ();
#endif
    }
}
