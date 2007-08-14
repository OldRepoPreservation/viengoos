/* ia32-shutdown.c - Shutdown routines for the ia32.
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

#include <sys/io.h>

#include "shutdown.h"


void
halt (void)
{
  while (1)
    asm volatile ("hlt");
}


/* There are three ways to reset an ia32 machine.  The first way is to
   make the corresponding BIOS call in real mode.  The second way is
   to program the keyboard controller to do it.  The third way is to
   triple fault the CPU by using an empty IDT and then causing a
   fault.  Any of these can fail on odd hardware.  */

void
reset (void)
{
  /* We only try to program the keyboard controller.  But if that
     fails, we should try to triple fault.  Alternatively, we could
     also try to make the BIOS call.  */

  outb_p (0x80, 0x70);
  inb_p (0x71);

  while (inb (0x64) & 0x02)
    ;

  outb_p (0x8F, 0x70);
  outb_p (0x00, 0x71);

  outb_p (0xFE, 0x64);
}
