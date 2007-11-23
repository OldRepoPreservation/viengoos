/* mm-init.h - Memory management initialization.
   Copyright (C) 2004, 2005 Free Software Foundation, Inc.
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
   along with the GNU Hurd; see the file COPYING.  If not, write to
   the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139,
   USA.  */

#if HAVE_CONFIG_H
#include <config.h>
#endif

#include <hurd/startup.h>
#include <hurd/exceptions.h>

#include "storage.h"
#include "as.h"

extern struct hurd_startup_data *__hurd_startup_data;

addr_t meta_data_activity;

void
mm_init (addr_t activity)
{
  extern int output_debug;

  output_debug = 4;

  if (ADDR_IS_VOID (activity))
    meta_data_activity = __hurd_startup_data->activity;
  else
    meta_data_activity = activity;

  storage_init ();
  as_init ();
  exception_handler_init ();
}
