/* stubs-init.h - Initialize system stubs in an architecture dependent way.
   Copyright (C) 2003 Free Software Foundation, Inc.
   Written by Marcus Brinkmann <marcus@gnu.org>.

   This file is part of the GNU L4 library.
 
   The GNU L4 library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public License
   as published by the Free Software Foundation; either version 2.1 of
   the License, or (at your option) any later version.
 
   The GNU L4 library is distributed in the hope that it will be
   useful, but WITHOUT ANY WARRANTY; without even the implied warranty
   of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU Lesser General Public License for more details.
 
   You should have received a copy of the GNU Lesser General Public
   License along with the GNU L4 library; if not, write to the Free
   Software Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
   02111-1307, USA.  */

#ifndef _L4_STUBS_INIT_H
# error "Never use <l4/bits/stubs-init.h> directly; include <l4/stubs-init.h> instead."
#endif

#define __L4_SETUP_SYSCALL(name)					\
extern void (*__l4_ ## name) (void);					\
  __l4_ ## name = (void (*) (void))					\
    (((l4_word_t) l4_kip ()) + l4_kip ()->name)


/* Initialize the syscall stubs.  */
static inline void
__attribute__((__always_inline__))
l4_init_stubs (void)
{
  __L4_SETUP_SYSCALL (exchange_registers);
  __L4_SETUP_SYSCALL (thread_control);
  __L4_SETUP_SYSCALL (system_clock);
  __L4_SETUP_SYSCALL (thread_switch);
  __L4_SETUP_SYSCALL (schedule);
  __L4_SETUP_SYSCALL (ipc);
  __L4_SETUP_SYSCALL (lipc);
  __L4_SETUP_SYSCALL (unmap);
  __L4_SETUP_SYSCALL (space_control);
  __L4_SETUP_SYSCALL (processor_control);
  __L4_SETUP_SYSCALL (memory_control);
};
