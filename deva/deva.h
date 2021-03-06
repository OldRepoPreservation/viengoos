/* deva.h - Generic definitions.
   Copyright (C) 2004 Free Software Foundation, Inc.
   Written by Marcus Brinkmann.

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

#include <errno.h>

#include <l4.h>
#include <hurd/cap-server.h>

#include "output.h"


/* The program name.  */
extern char program_name[];

#define BUG_ADDRESS	"<bug-hurd@gnu.org>"

int main (int argc, char *argv[]);


/* The following function must be defined by the architecture
   dependent code.  */

/* Switch execution transparently to thread TO.  The thread FROM,
   which must be the current thread, will be halted.  */
void switch_thread (l4_thread_id_t from, l4_thread_id_t to);


/* Device objects.  */

/* Initialize the device class subsystem.  */
error_t device_class_init ();

enum device_type
  {
    DEVICE_CONSOLE = 0,
    DEVICE_SERIAL = 1
  };

/* Allocate a new device object.  The object returned is locked and
   has one reference.  */
error_t device_alloc (hurd_cap_obj_t *r_obj, enum device_type type);
