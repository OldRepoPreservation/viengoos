/* output-stdio.c - A unix stdio output driver.
   Copyright (C) 2007 Free Software Foundation, Inc.
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

#if HAVE_CONFIG_H
#include <config.h>
#endif

#include "output.h"

#include <stdio.h>
#include <unistd.h>

static void
stdio_putchar (struct output_driver *device, int chr)
{
  char c[1] = { chr };
  write (1, &c, 1);
}


struct output_driver stdio_output =
  {
    "stdio",
    0,		/* init */
    0,		/* deinit */
    stdio_putchar
  };

/* A list of all output drivers, terminated with a null pointer.  */
struct output_driver *output_drivers[] =
  {
    &stdio_output,
    0
  };
