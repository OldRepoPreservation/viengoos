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

#include "deva.h"
#include "device.h"


static void
device_reinit (hurd_cap_class_t cap_class, hurd_cap_obj_t obj)
{
  device_t *dev = hurd_cap_obj_to_user (device_t *, obj);

  /* FIXME: Release resources.  */
}


static error_t
device_io_read (hurd_cap_rpc_context_t ctx)
{
  error_t err;
  device_t *dev = hurd_cap_obj_to_user (device_t *, ctx->obj);
  int chr;

  err =  (*dev->io_read) (dev, &chr);
  if (err)
    return err;

  /* Prepare reply message.  */
  l4_msg_clear (ctx->msg);
  l4_msg_append_word (ctx->msg, (l4_word_t) chr);

  return 0;
}


static error_t
device_io_write (hurd_cap_rpc_context_t ctx)
{
  device_t *dev = hurd_cap_obj_to_user (device_t *, ctx->obj);
  int chr;

  chr = (int) l4_msg_word (ctx->msg, 1);

  return (*dev->io_write) (dev, chr);
}


static error_t
device_demuxer (hurd_cap_rpc_context_t ctx)
{
  error_t err = 0;

  switch (l4_msg_label (ctx->msg))
    {
      /* DEVICE_IO_READ */
    case 768:
      err = device_io_read (ctx);
      break;

      /* DEVICE_IO_WRITE */
    case 769:
      err = device_io_write (ctx);
      break;

    default:
      err = EOPNOTSUPP;
    }

  return err;
}



static struct hurd_cap_class device_class;

/* Initialize the device class subsystem.  */
error_t
device_class_init ()
{
  return hurd_cap_class_init (&device_class, device_t *,
			      NULL, NULL, device_reinit, NULL,
			      device_demuxer);
}


/* Allocate a new device object.  The object returned is locked and
   has one reference.  */
error_t
device_alloc (hurd_cap_obj_t *r_obj, enum device_type type)
{
  error_t err;
  hurd_cap_obj_t obj;
  device_t *dev;

  err = hurd_cap_class_alloc (&device_class, &obj);
  if (err)
    return err;

  dev = hurd_cap_obj_to_user (device_t *, obj);

  switch (type)
    {
    case DEVICE_CONSOLE:
      device_console_init (dev);
      break;

    case DEVICE_SERIAL:
      device_serial_init (dev);
      break;

    default:
      break;
    }

  *r_obj = obj;
  return 0;
}
