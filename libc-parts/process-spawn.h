/* process-spawn.h - Process spawn interface.
   Copyright (C) 2008 Free Software Foundation, Inc.
   Written by Neal H. Walfield <neal@gnu.org>.

   This file is part of the GNU Hurd.

   The GNU Hurd is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 3 of the
   License, or (at your option) any later version.

   The GNU Hurd is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see
   <http://www.gnu.org/licenses/>.  */

#include <viengoos/addr.h>
#include <viengoos/thread.h>

/* Load a task.  Return a capability designating the main thread.  The
   slot is allocated with capalloc.  If MAKE_RUNNABLE is true, makes
   the process' thread runnable.  Otherwise, the thread remains
   suspended and may be started with thread_start.  */
extern thread_t process_spawn (addr_t activity,
			       void *start, void *end,
			       const char *const argv[],
			       const char *const env[],
			       bool make_runnable);
