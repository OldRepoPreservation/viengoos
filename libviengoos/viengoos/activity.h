/* activity.h - Activity definitions.
   Copyright (C) 2007, 2008 Free Software Foundation, Inc.
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

#ifndef _VIENGOOS_ACTIVITY_H
#define _VIENGOOS_ACTIVITY_H 1

#include <stdint.h>

enum
  {
    VG_activity_policy = 700,
    VG_activity_info,
  };

struct vg_activity_memory_policy
{
  uint16_t priority;
  uint16_t weight;
};

#define VG_ACTIVITY_MEMORY_POLICY(__amp_priority, __amp_weight)		\
  (struct vg_activity_memory_policy) { __amp_priority, __amp_weight }
#define VG_ACTIVITY_MEMORY_POLICY_VOID VG_ACTIVITY_MEMORY_POLICY(0, 0)

struct vg_activity_policy
{
  /* This policy is typically set by the parent to reflect how
     available memory should be distributed among its immediate
     children.  It may only be set via an activity control
     capability.  */
  struct vg_activity_memory_policy sibling_rel;

  /* This policy is typically set by the activity user and controls
     how the memory allocated *directly* to this activity is managed
     relative to the memory allocated to this activity's children.
     That is, if the activity has been choosen as a victim, this
     provides a policy to determine whether the memory allocated
     directly to the activity or that to a child activity should be
     evicted.  */
  struct vg_activity_memory_policy child_rel;

  /* Number of folios.  Zero means no limit.  (This does not mean that
     there is no limit, just that this activity does not impose a
     limit.  The parent activity, for instance, may impose a limit.)
     May only be set via an activity control capability.  */
  uint32_t folios;
};

/* Activity statistics.  These are approximate and in some cases
   represent averages.  */
#define VG_ACTIVITY_STATS_PERIODS 2
struct vg_activity_stats
{
  /* The period during which this statistic was generated.  */
  uint32_t period;

  /* The maximum number of frames this activity could currently
     allocate assuming other allocations do not otherwise change.
     This implies stealing from others.  */
  uint32_t available;
  uint32_t available_local;
  
  /* Log2 the maximum amount of memory (in pages) that the user of
     this activity ought to allocate in the next few seconds.  If
     negative, the amount of memory the activity ought to consider
     freeing.  */
  int8_t damping_factor;

  /* If pressure is non-zero, then this activity is causing PRESSURE.

     PRESSURE is calculated as follows: if 

       1) this activity is within its entitlement
       2) its working set is significantly smaller than its allocation
          (as determined by the size of inactive relative to active), and
       3) other activities are being held back (i.e., paging) due to this
          activity,

     then this represents the amount of memory it would be nice to see
     this activity free.  This activity will not be penalized by the
     system if it does not yield memory.  However, if the activity has
     memory which is yielding a low return, it would be friendly of it
     to return it.  */
  uint8_t pressure;
  uint8_t pressure_local;

  /* The number of clean and dirty frames that are accounted to this
     activity.  (Does not include frames scheduled for eviction.)  The
     total number of frames accounted to this activity is thus CLEAN +
     DIRTY.  */
  uint32_t clean;
  uint32_t dirty;
  /* Number of frames pending eviction.  */
  uint32_t pending_eviction;


  /* Based on recency information, the number of active frames
     accounted to this activity and its children.  The number of
     inactive frames is approximately CLEAN + DIRTY - ACTIVE.  */
  uint32_t active;
  /* Likewise, but excluding its children.  */
  uint32_t active_local;

  /* Number of frames that were active in the last period that become
     inactive in this period.  */
  uint32_t became_active;
  /* Number of frames that were inactive in the last period that
     become active in this period.  */
  uint32_t became_inactive;


  /* Number of frames that were not accounted to this activity in the
     last period and are now accounted to it.  */
  uint32_t claimed;
  /* Number of frames that were accounted to this activity in the last
     period and are no longer accounted to it.  */
  uint32_t disowned;

  /* The number of frames that this activity referenced but which are
     accounted to some other activity.  */
  uint32_t freeloading;
  /* The sum of the references by other processes to the frames that
     are accounted to this activity.  (A single frame may account
     for multiple references.)  */
  uint32_t freeloaded;


  /* Number of frames that were accounted to this activity and
     scheduled for eviction.  */
  uint32_t evicted;
  /* Number of frames that were accounted to this activity (not its
     children), had the discarded bit set, and were discarded.  */
  uint32_t discarded;
  /* Number of frames paged-in on behalf of this activity.  This does
     not include pages marked empty that do not require disk
     activity.  */
  uint32_t pagedin;
  /* Number of frames that were referenced before being completely
     freed.  (If evicted is significant and saved approximates
     evicted, then the process is trashing.)  */
  uint32_t saved;
};

#define VG_ACTIVITY_POLICY(__ap_sibling_rel, __ap_child_rel, __ap_storage) \
  (struct vg_activity_policy) { __ap_sibling_rel, __ap_child_rel, __ap_storage }
#define VG_ACTIVITY_POLICY_VOID			\
  VG_ACTIVITY_POLICY(VG_ACTIVITY_MEMORY_POLICY_VOID,	\
		  VG_ACTIVITY_MEMORY_POLICY_VOID,	\
		  0)

#define RPC_STUB_PREFIX vg
#define RPC_ID_PREFIX VG

#include <viengoos/rpc.h>

enum
{
  VG_ACTIVITY_POLICY_CHILD_REL_PRIORITY_SET = 1 << 0,
  VG_ACTIVITY_POLICY_CHILD_REL_WEIGHT_SET = 1 << 1,
  VG_ACTIVITY_POLICY_SIBLING_REL_PRIORITY_SET = 1 << 2,
  VG_ACTIVITY_POLICY_SIBLING_REL_WEIGHT_SET = 1 << 3,
  VG_ACTIVITY_POLICY_STORAGE_SET = 1 << 4,

  VG_ACTIVITY_POLICY_CHILD_REL_SET = (VG_ACTIVITY_POLICY_CHILD_REL_PRIORITY_SET
				   | VG_ACTIVITY_POLICY_CHILD_REL_WEIGHT_SET),

  VG_ACTIVITY_POLICY_SIBLING_REL_SET = (VG_ACTIVITY_POLICY_SIBLING_REL_PRIORITY_SET
				     | VG_ACTIVITY_POLICY_SIBLING_REL_WEIGHT_SET),
};

/* Get ACTIVITY's policy and set according to FLAGS and IN.  */
RPC (activity_policy, 2, 1, 0,
     /* cap_t principal, cap_t activity */
     uintptr_t, flags, struct vg_activity_policy, in,
     /* Out: */
     struct vg_activity_policy, out);

enum
  {
    /* Return statistics.  */
    vg_activity_info_stats = 1 << 0,
    /* Asynchronous change in availability.  */
    vg_activity_info_pressure = 1 << 1,
  };

struct vg_activity_info
{
  /* The returned event.  */
  uintptr_t event;
  union
  {
    /* If EVENT is vg_activity_info_stats.  */
    struct
    {
      /* The number of samples.  */
      int count;
      /* Samples are ordered by recency with the youngest towards the
	 start of the buffer.  */
      struct vg_activity_stats stats[VG_ACTIVITY_STATS_PERIODS];
    } stats;

    /* If EVENT is activity_info_free.  */
    struct
    {
      /* The number of pages the caller should try to free (negative)
	 or may allocate (positive).  */
      int amount;
    } pressure;
  };
};

/* Return some information about the activity ACTIVITY.  FLAGS is a
   bit-wise or of events the caller is interested.  Only one event
   will be returned.

   If FLAGS contains vg_activity_info_stats, may return the next
   statistic that comes at or after UNTIL_PERIOD.  (This can be used
   to register a callback that is sent when the statistics are next
   available.  For example, call with UNTIL_PERIOD equal to 0 to get
   the current statistics and then examine the period field.  Use this
   as the base for the next call.)

   If FLAGS contains activity_info_free, may return an upcall
   indicating that the activity must free some memory or will be such
   subject to paging.  In this case, the activity should try to free
   at least the indicated number of pages as quickly as possible.  */
RPC (activity_info, 2, 1, 0,
     /* cap_t principal, cap_t activity, */
     uintptr_t, flags, uintptr_t, until_period,
     /* Out: */
     struct vg_activity_info, info)

#undef RPC_STUB_PREFIX
#undef RPC_ID_PREFIX
#undef RPC_TARGET

#endif
