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
extern void __attribute__ ((__noreturn__)) shutdown_machine (void);

#endif	/* _SHUTDOWN_H */
