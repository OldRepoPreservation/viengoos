/* activity.h - Activity definitions.
   Copyright (C) 2007 Free Software Foundation, Inc.
   Written by Neal H. Walfield <neal@gnu.org>.

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

#ifndef _HURD_ACTIVITY_H
#define _HURD_ACTIVITY_H 1

#include <hurd/types.h>
#include <hurd/startup.h>

enum
  {
    RM_activity_create = 700,
  };

#define RPC_STUB_PREFIX rm
#define RPC_ID_PREFIX RM
#undef RPC_TARGET_NEED_ARG
#define RPC_TARGET \
  ({ \
    extern struct hurd_startup_data *__hurd_startup_data; \
    __hurd_startup_data->rm; \
  })

#include <hurd/rpc.h>

/* Create an activity at the activity denoted by ACTIVITY (ACTIVITY
   must be an activity control cap).  The new activity will be a child
   of PARENT.  It will be in the priority class PRIORITY (0 = highest
   priority) and have weight WEIGHT (= proportion of resources
   available to the child activities in this priority class).  Its
   storage quota indicates the number of folios available to this
   activity (0 = no limit).

   If not ADDR_VOID, an activity capability is saved in the capability
   slot designated by ACTIVITY_OUT and a control activity in the slot
   designated by ACTIVITY_CONTROL_OUT.  */
RPC7 (activity_create, addr_t, parent, addr_t, activity, 
      l4_word_t, priority, l4_word_t, weight,
      l4_word_t, storage_quota,
      addr_t, activity_out, addr_t, activity_control_out)

#undef RPC_STUB_PREFIX
#undef RPC_ID_PREFIX
#undef RPC_TARGET

#endif
