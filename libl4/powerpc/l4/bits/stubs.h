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

/* This file can be used externally to define the system call stubs,
   or internally to declare them (by defining _L4_EXTERN_STUBS).  */

#if !defined(_L4_STUBS_H) && !defined(_L4_EXTERN_STUBS)
# error "Never use <l4/bits/stubs.h> directly; include <l4/stubs.h> instead."
#endif

#ifdef _L4_EXTERN_STUBS
# define _L4_EXTERN extern
#else
# define _L4_EXTERN
#endif

#ifndef __l4_syscall_stub_t
typedef void (*__l4_syscall_stub_t) (void);
# define __l4_syscall_stub_t __l4_syscall_stub_t
#endif

_L4_EXTERN __l4_syscall_stub_t __l4_exchange_registers;
_L4_EXTERN __l4_syscall_stub_t __l4_thread_control;
_L4_EXTERN __l4_syscall_stub_t __l4_system_clock;
_L4_EXTERN __l4_syscall_stub_t __l4_thread_switch;
_L4_EXTERN __l4_syscall_stub_t __l4_schedule;
_L4_EXTERN __l4_syscall_stub_t __l4_ipc;
_L4_EXTERN __l4_syscall_stub_t __l4_lipc;
_L4_EXTERN __l4_syscall_stub_t __l4_unmap;
_L4_EXTERN __l4_syscall_stub_t __l4_space_control;
_L4_EXTERN __l4_syscall_stub_t __l4_processor_control;
_L4_EXTERN __l4_syscall_stub_t __l4_memory_control;

#undef _L4_EXTERN
