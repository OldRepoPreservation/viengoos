/* Set a thread's activity activity.  Viengoos version.
   Copyright (C) 2008 Free Software Foundation, Inc.
   This file is part of the GNU C Library.

   The GNU C Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   The GNU C Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public
   License along with the GNU C Library; see the file COPYING.LIB.  If not,
   write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.  */

#include <pt-internal.h>

#include <viengoos/addr.h>
#include <viengoos/thread.h>

int
pthread_setactivity_np (vg_addr_t activity)
{
  struct __pthread *self = _pthread_self ();

  struct vg_thread_exregs_in in;
  struct vg_thread_exregs_out out;
  int err = vg_thread_exregs (VG_ADDR_VOID, self->object,
			      VG_EXREGS_SET_ACTIVITY,
			      in, VG_ADDR_VOID, activity, VG_ADDR_VOID, VG_ADDR_VOID,
			      &out, NULL, NULL, NULL, NULL);

  return err;
}
