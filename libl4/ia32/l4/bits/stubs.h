/* stubs.h - L4 system call stubs for powerpc.
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

#ifndef _L4_STUBS_H
# error "Never use <l4/bits/stubs.h> directly; include <l4/stubs.h> instead."
#endif

typedef void (*__l4_syscall_stub_t) (void);

__l4_syscall_stub_t __l4_exchange_registers;
__l4_syscall_stub_t __l4_thread_control;
__l4_syscall_stub_t __l4_system_clock;
__l4_syscall_stub_t __l4_thread_switch;
__l4_syscall_stub_t __l4_schedule;
__l4_syscall_stub_t __l4_ipc;
__l4_syscall_stub_t __l4_lipc;
__l4_syscall_stub_t __l4_unmap;
__l4_syscall_stub_t __l4_space_control;
__l4_syscall_stub_t __l4_processor_control;
__l4_syscall_stub_t __l4_memory_control;
