/* Main function for the task server.
   Copyright (C) 2004 Free Software Foundation, Inc.
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
#include <pthread.h>

#include "task.h"


/* The program name.  */
char program_name[] = "task";


/* The following functions are required by pthread.  */

void
__attribute__ ((__noreturn__))
exit (int __status)
{
  panic ("exit() called");
}


void
abort (void)
{
  panic ("abort() called");
}


#define WORTEL_MSG_PUTCHAR		1
#define WORTEL_MSG_PANIC		2
#define WORTEL_MSG_GET_MEM		3
#define WORTEL_MSG_GET_CAP_REQUEST	4
#define WORTEL_MSG_GET_CAP_REPLY	5
#define WORTEL_MSG_GET_THREADS		6
#define WORTEL_MSG_GET_TASK_CAP		7


/* FIXME:  Should be elsewhere.  Needed by libhurd-slab.  */
int
getpagesize()
{
  return l4_min_page_size ();
}


int
main (int argc, char *argv[])
{
  error_t err;
  l4_thread_id_t server_thread;
  hurd_cap_bucket_t bucket;
  pthread_t manager;

  output_debug = 1;

  debug ("%s " PACKAGE_VERSION "\n", program_name);

  while (1)
    l4_sleep (L4_NEVER);

  return 0;
}
