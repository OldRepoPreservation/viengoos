/* hurd/types.h - Basic types for the GNU Hurd.
   Copyright (C) 2004 Free Software Foundation, Inc.
   Written by Marcus Brinkmann <marcus@gnu.org>

   This file is part of the GNU Hurd.

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with this program; if not, write to the Free
   Software Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
   02111-1307 USA.  */

#ifndef _HURD_TYPES_H
#define _HURD_TYPES_H	1

#include <l4/types.h>
#include <l4/thread.h>


/* The number of bits that are valid for the task ID.  */
#define HURD_TASK_ID_BITS	L4_THREAD_VERSION_BITS

/* The Hurd task ID is always stored in the version part of the thread ID.  */
typedef l4_word_t hurd_task_id_t;

#define HURD_TASK_ID_NULL	(0)

static inline hurd_task_id_t
__attribute__((always_inline))
hurd_task_id_from_thread_id (l4_thread_id_t tid)
{
  return l4_version (tid);
}


/* Client-side capability handle.  */
typedef l4_word_t hurd_cap_handle_t;

#endif	/* _HURD_TYPES_H */
