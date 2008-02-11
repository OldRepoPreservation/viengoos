/* panic.h - Panic implementation.
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

#include <hurd/stddef.h>
#include "output.h"
#include "shutdown.h"
#include "debug.h"

extern char *program_name;

extern int backtrace (void **array, int size);

void
panic_ (const char *func, int line, const char *fmt, ...)
{
  va_list ap;

  va_start (ap, fmt);

  printf ("%s:%s:%d error: ", program_name, func, line);
  vprintf (fmt, ap);
  putchar ('\n');
  va_end (ap);

  void *a[10];
  int count = backtrace (a, sizeof (a) / sizeof (a[0]));
  int i;
  printf ("Backtrace: ");
  for (i = 0; i < count; i ++)
    printf ("%p ", a[i]);
  printf ("\n");

  debugger ();
  shutdown_machine ();
}

