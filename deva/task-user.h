/* task-user.h - User stub interfaces for task RPCs.
   Copyright (C) 2004 Free Software Foundation, Inc.
   Written by Marcus Brinkmann.

   This file is part of the GNU Hurd.

   The GNU Hurd is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.

   The GNU Hurd is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with the GNU C Library; if not, write to the Free
   Software Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
   02111-1307 USA.  */

#ifndef HURD_TASK_USER_H
#define HURD_TASK_USER_H	1

#include <l4.h>

#include <sys/types.h>

#include <hurd/types.h>


/* Allocate a new thread for the task TASK, and return its thread ID
   in THREAD_ID.  */
error_t task_thread_alloc (l4_thread_id_t task_server, hurd_cap_id_t task,
			   void *utcb, l4_thread_id_t *thread_id);

#endif	/* HURD_TASK_USER_H */
