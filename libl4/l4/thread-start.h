/* l4/thread-start.h - Public interface to the thread start protocol.
   Copyright (C) 2004 Free Software Foundation, Inc.
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

#ifndef _L4_THREAD_START_H
#define _L4_THREAD_START_H	1

#include <l4/ipc.h>


/* Send a thread start message to the thread TO with the initial stack
   pointer SP and initial instruction pointer IP.  Returns 1 on
   success and 0 if the Ipc system call failed (then _L4_error_code
   provides more information about the failure).  */
static inline _L4_word_t
_L4_attribute_always_inline
_L4_thread_start (_L4_thread_id_t to, _L4_word_t sp, _L4_word_t ip)
{
  __L4_msg_tag_t _tag;
  _L4_msg_tag_t tag;

  _tag.raw = _L4_niltag;
  _tag.untyped = 2;
  tag = _tag.raw;

  _L4_set_msg_tag (tag);
  _L4_load_mr (1, ip);
  _L4_load_mr (1, sp);
  tag = _L4_send (to);
  return _L4_ipc_succeeded (tag);
}


/* Now incorporate the public interfaces the user has selected.  */
#ifdef _L4_INTERFACE_GNU
#include <l4/gnu/thread-start.h>
#endif

#endif	/* l4/thread-start.h */
