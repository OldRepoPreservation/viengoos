/* ia32-cmain.c - Startup code for the ia32.
   Copyright (C) 2003, 2004, 2005, 2008 Free Software Foundation, Inc.
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

#include <alloca.h>
#include <stdint.h>
#include <string.h>

#include <l4/globals.h>
#include <l4/init.h>
#include <l4/stubs.h>
#include <l4/stubs-init.h>

#include <hurd/startup.h>
#include <hurd/mm.h>
#include <hurd/stddef.h>


/* Initialized by the machine-specific startup-code.  */
extern struct hurd_startup_data *__hurd_startup_data;


extern void exit (int status)  __attribute__ ((__noreturn__));
extern int main (int, char *[]);

char *program_name = "in crt0";

static void
finish (void)
{
  int argc = 0;
  char **argv = 0;

  char *str = __hurd_startup_data->argz;
  if (str)
    /* A command line was passed.  */
    {
      int nr = 0;

      /* First time around we count the number of arguments.  */
      argc = 1;
      while (*str && *str == ' ')
	str++;

      while (*str)
	if (*(str++) == ' ')
	  {
	    while (*str && *str == ' ')
	      str++;
	    if (*str)
	      argc++;
	  }
      argv = alloca (sizeof (char *) * (argc + 1));

      /* Second time around we fill in the argv.  */
      str = (char *) __hurd_startup_data->argz;

      while (*str && *str == ' ')
	str++;
      argv[nr++] = str;

      while (*str)
	{
	  if (*str == ' ')
	    {
	      *(str++) = '\0';
	      while (*str && *str == ' ')
		str++;
	      if (*str)
		argv[nr++] = str;
	    }
	  else
	    str++;
	}
      argv[nr] = 0;
    }
  else
    {
      argc = 1;

      argv = alloca (sizeof (char *) * 2);
      argv[0] = (char *) program_name;
      argv[1] = 0;
    }

  program_name = "unknown";
  if (argv[0])
    program_name = (strrchr (argv[0], '/')
		    ? strrchr (argv[0], '/') + 1 : argv[0]);

  /* Now invoke the main function.  */
  exit (main (argc, argv));
}

/* Initialize libl4, setup the argument vector, and pass control over
   to the main function.  */
void
cmain (void)
{
  l4_init ();
  l4_init_stubs ();

  s_printf ("In cmain\n");

  mm_init (__hurd_startup_data->activity);

  extern void (*_pthread_init_routine)(void (*entry) (void *), void *)
    __attribute__ ((noreturn));
  if (_pthread_init_routine)
    _pthread_init_routine (finish, NULL);
  else
    finish ();

  /* Never reached.  */
}
