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
#include <hurd/addr.h>

enum
  {
    RM_activity_properties = 700,
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

enum
{
  ACTIVITY_PROPERTIES_PRIORITY_SET = 1 << 0,
  ACTIVITY_PROPERTIES_WEIGHT_SET = 1 << 1,
  ACTIVITY_PROPERTIES_STORAGE_QUOTA_SET = 1 << 2,

  ACTIVITY_PROPERTIES_ALL_SET = (ACTIVITY_PROPERTIES_PRIORITY_SET
				 | ACTIVITY_PROPERTIES_WEIGHT_SET
				 | ACTIVITY_PROPERTIES_STORAGE_QUOTA_SET),
};

RPC (activity_properties, 5, 3, addr_t, activity, l4_word_t, flags,
     l4_word_t, priority, l4_word_t, weight, l4_word_t, storage_quota,
     /* Out: */
     l4_word_t, priority_old, l4_word_t, weight_old,
     l4_word_t, storage_quota_old)

#undef RPC_STUB_PREFIX
#undef RPC_ID_PREFIX
#undef RPC_TARGET

#endif
