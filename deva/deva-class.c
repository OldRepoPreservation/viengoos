/* deva-class.c - Deva class for the deva server.
   Copyright (C) 2003, 2004 Free Software Foundation, Inc.
   Written by Marcus Brinkmann.

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

#if HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdlib.h>

#include <l4.h>
#include <hurd/cap-server.h>
#include <hurd/wortel.h>

#include "deva.h"


struct deva
{
  /* FIXME: More stuff.  */
  int foo;
};
typedef struct deva *deva_t;


static void
deva_reinit (hurd_cap_class_t cap_class, hurd_cap_obj_t obj)
{
  deva_t deva = hurd_cap_obj_to_user (deva_t, obj);

  /* FIXME: Release resources.  */
}


error_t
deva_io_write (hurd_cap_rpc_context_t ctx)
{
  return 0;
}


error_t
deva_demuxer (hurd_cap_rpc_context_t ctx)
{
  error_t err = 0;

  switch (l4_msg_label (ctx->msg))
    {
      /* DEVA_IO_WRITE */
    case 256:
      err = deva_io_write (ctx);
      break;

    default:
      err = EOPNOTSUPP;
    }

  return err;
}



static struct hurd_cap_class deva_class;

/* Initialize the deva class subsystem.  */
error_t
deva_class_init ()
{
  return hurd_cap_class_init (&deva_class, deva_t,
			      NULL, NULL, deva_reinit, NULL,
			      deva_demuxer);
}


/* Allocate a new deva object.  The object returned is locked and has
   one reference.  */
error_t
deva_alloc (hurd_cap_obj_t *r_obj)
{
  error_t err;
  hurd_cap_obj_t obj;
  deva_t deva;

  err = hurd_cap_class_alloc (&deva_class, &obj);
  if (err)
    return err;

  deva = hurd_cap_obj_to_user (deva_t, obj);

  /* FIXME: Add some stuff.  */

  *r_obj = obj;
  return 0;
}
