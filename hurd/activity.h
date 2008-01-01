/* activity.h - Activity definitions.
   Copyright (C) 2007 Free Software Foundation, Inc.
   Written by Neal H. Walfield <neal@gnu.org>.

   This file is part of the GNU Hurd.

   GNU Hurd is free software: you can redistribute it and/or modify it
   under the terms of the GNU Lesser General Public License as
   published by the Free Software Foundation, either version 3 of the
   License, or (at your option) any later version.

   GNU Hurd is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with GNU Hurd.  If not, see
   <http://www.gnu.org/licenses/>.  */

#ifndef _HURD_ACTIVITY_H
#define _HURD_ACTIVITY_H 1

#include <hurd/types.h>
#include <hurd/startup.h>
#include <hurd/addr.h>

enum
  {
    RM_activity_policy = 700,
  };

struct activity_memory_policy
{
  uint16_t weight;
  uint16_t priority;
};

#define ACTIVITY_MEMORY_POLICY(__amp_weight, __amp_priority) \
  (struct activity_memory_policy) { __amp_weight, __amp_priority }
#define ACTIVITY_MEMORY_POLICY_VOID ACTIVITY_MEMORY_POLICY(0, 0)

struct activity_policy
{
  /* This policy is typically set by the parent to reflect how
     available memory should be distributed among its immediate
     children.  It may only be set via an activity control
     capability.  */
  struct activity_memory_policy sibling_rel;

  /* This policy is typically set by the activity user and controls
     how the memory allocated *directly* to this activity is managed
     relative to the memory allocated to this activity's children.
     That is, if the activity has been choosen as a victim, this
     provides a policy to determine whether the memory allocated
     directly to the activity or that to a child activity should be
     evicted.  */
  struct activity_memory_policy child_rel;

  /* Number of folios.  Zero means no limit.  (This does not mean that
     there is no limit, just that this activity does not impose a
     limit.  The parent activity, for instance, may impose a limit.)
     May only be set via an activity control capability.  */
  uint32_t folios;
};

#define ACTIVITY_POLICY(__ap_sibling_rel, __ap_child_rel, __ap_storage) \
  (struct activity_policy) { __ap_sibling_rel, __ap_child_rel, __ap_storage }
#define ACTIVITY_POLICY_VOID			\
  ACTIVITY_POLICY(ACTIVITY_MEMORY_POLICY_VOID,	\
		  ACTIVITY_MEMORY_POLICY_VOID,	\
		  0)

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
  ACTIVITY_POLICY_CHILD_REL_PRIORITY_SET = 1 << 0,
  ACTIVITY_POLICY_CHILD_REL_WEIGHT_SET = 1 << 1,
  ACTIVITY_POLICY_SIBLING_REL_PRIORITY_SET = 1 << 2,
  ACTIVITY_POLICY_SIBLING_REL_WEIGHT_SET = 1 << 3,
  ACTIVITY_POLICY_STORAGE_SET = 1 << 4,

  ACTIVITY_POLICY_CHILD_REL_SET = (ACTIVITY_POLICY_CHILD_REL_PRIORITY_SET
				   | ACTIVITY_POLICY_CHILD_REL_WEIGHT_SET),

  ACTIVITY_POLICY_SIBLING_REL_SET = (ACTIVITY_POLICY_SIBLING_REL_PRIORITY_SET
				     | ACTIVITY_POLICY_SIBLING_REL_WEIGHT_SET),
};

/* Get ACTIVITY's policy and set according to FLAGS and IN.  */
RPC (activity_policy, 3, 1, addr_t, activity,
     uintptr_t, flags, struct activity_policy, in,
     /* Out: */
     struct activity_policy, out);

#undef RPC_STUB_PREFIX
#undef RPC_ID_PREFIX
#undef RPC_TARGET

#endif
