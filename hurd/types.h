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


/* The type used for a user-side capability ID.  */
typedef l4_word_t hurd_cap_t;

/* Every capability hurd_cap_t consists of two parts: The upper part
   is a client ID and the lower part is a capability object ID.  The
   client ID is as long as the task ID (which is as long as the
   version ID).  The cap ID occupies the remainder.  We intimately
   know that even on 64 bit architectures, both fit into a 32 bit
   integer value.  */
#define HURD_CAP_CLIENT_ID_BITS	HURD_TASK_ID_BITS
#define HURD_CAP_ID_BITS	((sizeof (hurd_cap_t) * 8) - HURD_TASK_ID_BITS)

typedef l4_uint32_t hurd_cap_id_t;
typedef l4_uint32_t hurd_cap_client_id_t;


/* Get the capability ID from a user capability.  The capabililty ID
   is an index into the caps table of a client.  */
static inline hurd_cap_client_id_t
hurd_cap_client_id (hurd_cap_t cap)
{
  return cap >> HURD_CAP_ID_BITS;
}


/* Get the capability ID from a user capability.  The capabililty ID
   is an index into the caps table of a client.  */
static inline hurd_cap_id_t
hurd_cap_id (hurd_cap_t cap)
{
  return cap & ((L4_WORD_C(1) << HURD_CAP_ID_BITS) - 1);
}

#endif	/* _HURD_TYPES_H */
