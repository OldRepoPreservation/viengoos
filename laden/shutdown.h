/* shutdown.h - System shutdown functions interfaces.
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

#ifndef _SHUTDOWN_H
#define _SHUTDOWN_H	1

#include "output.h"


/* Every architecture must provide the following functions.  */

/* Reset the machine.  */
void reset (void);

/* Halt the machine.  */
void halt (void);


/* The generic code defines these functions.  */

/* Reset the machine at failure, instead halting it.  */
extern int shutdown_reset;

/* End the program with a failure.  This can halt or reset the
   system.  */
void shutdown (void);

/* The program name.  */
extern char *program_name;

/* Print an error message and fail.  */
#define panic(...)				\
  ({						\
    printf ("%s: error: ", program_name);	\
    printf (__VA_ARGS__);			\
    putchar ('\n');				\
    shutdown ();				\
  })

#endif	/* _SHUTDOWN_H */
