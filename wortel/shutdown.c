/* shutdown.c - System shutdown functions.
   Copyright (C) 2003 Free Software Foundation, Inc.
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

#if HAVE_CONFIG_H
#include <config.h>
#endif

#include <l4.h>

#include "shutdown.h"


/* Reset the machine at failure, instead halting it.  */
int shutdown_reset;

/* Time to sleep before reset.  */
#define SLEEP_TIME 10


void
halt (void)
{
  l4_sleep (l4_never);
}


void
shutdown (void)
{
  if (shutdown_reset)
    {
      l4_time_t timespec = l4_time_period (SLEEP_TIME * 1000UL * 1000UL);

      l4_sleep (timespec);
      reset ();
    }
  else
    halt ();

  /* Never reached.  */
  if (shutdown_reset)
    {
      printf ("Unable to reset this machine.\n");
      halt ();
    }

  printf ("Unable to halt this machine.\n");
  while (1)
    ;
}
