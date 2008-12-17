/* vg_cap.h - Capability definitions.
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

#ifndef _VIENGOOS_CAP_H
#define _VIENGOOS_CAP_H 1

#include <hurd/types.h>
#include <hurd/stddef.h>
#include <viengoos/addr.h>
#include <viengoos/addr-trans.h>
#include <hurd/startup.h>
#include <hurd/error.h>
#include <stdint.h>
#include <stdbool.h>

/* Capabilities.

   Capabilities have three functions: a capability can designate an
   object, it can participate in address translation, and it can be
   used to control how the designated object should be managed.  */

/* The types of objects designated by capabilities.  */
enum vg_cap_type
  {
#define VG_CAP_TYPE_MIN vg_cap_void
    vg_cap_void,
    vg_cap_page,
    vg_cap_rpage,
    vg_cap_cappage,
    vg_cap_rcappage,
    vg_cap_folio,
    vg_cap_activity,
    vg_cap_activity_control,
    vg_cap_thread,
    vg_cap_messenger,
    vg_cap_rmessenger,
    vg_cap_type_count,
#define VG_CAP_TYPE_MAX (vg_cap_type_count - 1)
  };

static inline const char *
vg_cap_type_string (enum vg_cap_type type)
{
  switch (type)
    {
    case vg_cap_void:
      return "void";
    case vg_cap_page:
      return "page";
    case vg_cap_rpage:
      return "rpage";
    case vg_cap_cappage:
      return "cappage";
    case vg_cap_rcappage:
      return "rcappage";
    case vg_cap_folio:
      return "folio";
    case vg_cap_activity:
      return "activity";
    case vg_cap_activity_control:
      return "activity_control";
    case vg_cap_thread:
      return "thread";
    case vg_cap_messenger:
      return "messenger";
    case vg_cap_rmessenger:
      return "rmessenger";
    default:
      return "unknown vg_cap type";
  };
}

/* Return whether two types are compatible in the sense that two caps
   with the given types can designate the same object.  */
static inline bool
vg_cap_types_compatible (enum vg_cap_type a, enum vg_cap_type b)
{
  if (a == b)
    return true;

  if (a == vg_cap_page && b == vg_cap_rpage)
    return true;
  if (a == vg_cap_rpage && b == vg_cap_page)
    return true;

  if (a == vg_cap_cappage && b == vg_cap_rcappage)
    return true;
  if (a == vg_cap_rcappage && b == vg_cap_cappage)
    return true;

  if (a == vg_cap_activity && b == vg_cap_activity_control)
    return true;
  if (a == vg_cap_activity_control && b == vg_cap_activity)
    return true;

  if (a == vg_cap_messenger && b == vg_cap_rmessenger)
    return true;
  if (a == vg_cap_rmessenger && b == vg_cap_messenger)
    return true;

  return false;
}

/* Returns weather TYPE corresponds to a weak type.  */
static inline bool
vg_cap_type_weak_p (enum vg_cap_type type)
{
  switch (type)
    {
    case vg_cap_rpage:
    case vg_cap_rcappage:
    case vg_cap_activity:
    case vg_cap_rmessenger:
      return true;

    default:
      return false;
    }
}

/* Returns the weakened type corresponding to TYPE.  If type is
   already a weak type, returns TYPE.  */
static inline enum vg_cap_type
vg_cap_type_weaken (enum vg_cap_type type)
{
  switch (type)
    {
    case vg_cap_page:
    case vg_cap_rpage:
      return vg_cap_rpage;

    case vg_cap_cappage:
    case vg_cap_rcappage:
      return vg_cap_rcappage;

    case vg_cap_activity_control:
    case vg_cap_activity:
      return vg_cap_activity;

    case vg_cap_messenger:
    case vg_cap_rmessenger:
      return vg_cap_rmessenger;

    default:
      return vg_cap_void;
    }
}

/* Returns the strong type corresponding to TYPE.  If type is already
   a strong type, returns TYPE.  */
static inline enum vg_cap_type
vg_cap_type_strengthen (enum vg_cap_type type)
{
  switch (type)
    {
    case vg_cap_page:
    case vg_cap_rpage:
      return vg_cap_page;

    case vg_cap_cappage:
    case vg_cap_rcappage:
      return vg_cap_cappage;

    case vg_cap_activity_control:
    case vg_cap_activity:
      return vg_cap_activity_control;

    case vg_cap_messenger:
    case vg_cap_rmessenger:
      return vg_cap_messenger;

    default:
      return type;
    }
}

/* Object policy.  */

/* The object priority is a signed 7-bit number (-64 -> 63).  A lower
   numeric value corresponds to a lower priority.  */
#define VG_OBJECT_PRIORITY_BITS 7
#define VG_OBJECT_PRIORITY_LEVELS (1 << VG_OBJECT_PRIORITY_BITS)
#define VG_OBJECT_PRIORITY_MIN (-(1 << (VG_OBJECT_PRIORITY_BITS - 1)))
#define VG_OBJECT_PRIORITY_DEFAULT (0)
#define VG_OBJECT_PRIORITY_MAX ((1 << (VG_OBJECT_PRIORITY_BITS - 1)) - 1)

struct object_policy
{
  union
  {
    struct
    {
      /* Whether a page is discardable (if so and the page is not
	 zero, trying to read the page from disk generates a first
	 fault fault).  */
      int8_t discardable : 1;

      /* An object's priority.  If can be used to override LRU
	 eviction.  When a memory object is to be evicted, we select
	 the object with the lowest priority (higher value = lower
	 priority).  */
      int8_t priority : VG_OBJECT_PRIORITY_BITS;
    };
    uint8_t raw;
  };
};

#define VG_OBJECT_POLICY_INIT { { raw: 0 } }
#define VG_OBJECT_POLICY(__op_discardable, __op_priority) \
  (struct object_policy) { { { (__op_discardable), (__op_priority) } } }
/* The default object policy: not discardable, managed by LRU.  */
#define VG_OBJECT_POLICY_VOID \
  VG_OBJECT_POLICY (false,  VG_OBJECT_PRIORITY_DEFAULT)
/* Synonym for VG_OBJECT_POLICY_VOID.  */
#define VG_OBJECT_POLICY_DEFAULT VG_OBJECT_POLICY_VOID

/* Capability properties.  */

struct vg_cap_properties
{
  struct object_policy policy;
  struct vg_cap_addr_trans addr_trans;
};

#define VG_CAP_PROPERTIES_INIT \
  { VG_OBJECT_POLICY_INIT, VG_CAP_ADDR_TRANS_INIT }
#define VG_CAP_PROPERTIES(__op_object_policy, __op_addr_trans) \
  (struct vg_cap_properties) { __op_object_policy, __op_addr_trans }
#define VG_CAP_PROPERTIES_VOID \
  VG_CAP_PROPERTIES (VG_OBJECT_POLICY_INIT, VG_CAP_ADDR_TRANS_INIT)
#define VG_CAP_PROPERTIES_DEFAULT VG_CAP_PROPERTIES_VOID

/* Capability representation.  */

#ifdef RM_INTERN
/* An OID corresponds to a page on a volume.  Only the least 54 bits
   are significant.  */
typedef uint64_t vg_oid_t;
#define VG_OID_FMT "0x%llx"
#define VG_OID_PRINTF(__op_oid) ((vg_oid_t) (__op_oid))
#endif

#define VG_CAP_VERSION_BITS 20
#define VG_CAP_TYPE_BITS 6

struct vg_cap
{
#ifdef RM_INTERN
  /* For a description of how versioning works, refer to the comment
     titled "Object versioning" in object.h.  */
  uint32_t version : VG_CAP_VERSION_BITS;
  /* Whether the capability is weak.  */
  uint32_t weak_p : 1;

  /* Whether the designated object may be discarded.  */
  uint32_t discardable : 1;
  /* The designated object's priority.  */
  int32_t priority : VG_OBJECT_PRIORITY_BITS;

  struct vg_cap_addr_trans addr_trans;

  uint64_t type : VG_CAP_TYPE_BITS;

  /* If the capability designates an object, the object id.  */
  uint64_t oid : 64 - VG_CAP_TYPE_BITS;
#else
  /* The shadow object (only for cappages and folios).  */
  struct object *shadow;

  uint32_t discardable : 1;
  int32_t priority : VG_OBJECT_PRIORITY_BITS;

  uint32_t type : VG_CAP_TYPE_BITS;

  uint32_t pad0 : 32 - 1 - VG_OBJECT_PRIORITY_BITS - VG_CAP_TYPE_BITS;

  /* This capability's address description.  */
  struct vg_cap_addr_trans addr_trans;
#endif
};

#define VG_CAP_VOID ((struct vg_cap) { .type = vg_cap_void })

/* Return CAP's policy.  */
#define VG_CAP_POLICY_GET(__cpg_cap)				\
  VG_OBJECT_POLICY ((__cpg_cap).discardable, (__cpg_cap).priority)
/* Set CAP's policy to POLICY.  */
#define VG_CAP_POLICY_SET(__cps_cap, __cps_policy)			\
  do								\
    {								\
      (__cps_cap)->discardable = (__cps_policy).discardable;	\
      (__cps_cap)->priority = (__cps_policy).priority;		\
    }								\
  while (0)

/* Return CAP's properties.  */
#define VG_CAP_PROPERTIES_GET(__cpg_cap)				\
  VG_CAP_PROPERTIES (VG_CAP_POLICY_GET (__cpg_cap),			\
		  (__cpg_cap).addr_trans)
/* Set *CAP's properties to PROPERTIES.  */
#define VG_CAP_PROPERTIES_SET(__cps_cap, __cps_properties)			\
  do									\
    {									\
      VG_CAP_POLICY_SET (__cps_cap, (__cps_properties).policy);		\
      (__cps_cap)->addr_trans = (__cps_properties).addr_trans;		\
    }									\
  while (0)

/* Convenience macros for printing capabilities.  */

#ifdef RM_INTERN
#define VG_CAP_FMT "{ " VG_OID_FMT ".%d:%s %llx/%d; %d/%d }"
#define VG_CAP_PRINTF(vg_cap) \
  VG_OID_PRINTF ((vg_cap)->oid), (vg_cap)->version, vg_cap_type_string ((vg_cap)->type), \
  VG_CAP_GUARD ((vg_cap)), VG_CAP_GUARD_BITS ((vg_cap)), \
  VG_CAP_SUBPAGE ((vg_cap)), VG_CAP_SUBPAGES ((vg_cap))
#else
#define VG_CAP_FMT "{ %s %llx/%d; %d/%d }"
#define VG_CAP_PRINTF(vg_cap) \
  vg_cap_type_string ((vg_cap)->type), \
  VG_CAP_GUARD ((vg_cap)), VG_CAP_GUARD_BITS ((vg_cap)), \
  VG_CAP_SUBPAGE ((vg_cap)), VG_CAP_SUBPAGES ((vg_cap))
#endif

/* Accessors corresponding to the CAP_ADDR_TRANS macros.  */
#define VG_CAP_SUBPAGES_LOG2(cap_) \
  VG_CAP_ADDR_TRANS_SUBPAGES_LOG2((cap_)->addr_trans)
#define VG_CAP_SUBPAGES(cap_) VG_CAP_ADDR_TRANS_SUBPAGES ((cap_)->addr_trans)
#define VG_CAP_SUBPAGE(cap_) VG_CAP_ADDR_TRANS_SUBPAGE((cap_)->addr_trans)
#define VG_CAP_SUBPAGE_SIZE_LOG2(cap_) \
  VG_CAP_ADDR_TRANS_SUBPAGE_SIZE_LOG2 ((cap_)->addr_trans)
#define VG_CAP_SUBPAGE_SIZE(cap_) \
  VG_CAP_ADDR_TRANS_SUBPAGE_SIZE ((cap_)->addr_trans)
#define VG_CAP_SUBPAGE_OFFSET(cap_) \
  VG_CAP_ADDR_TRANS_SUBPAGE_OFFSET((cap_)->addr_trans)
#define VG_CAP_GUARD_BITS(cap_) VG_CAP_ADDR_TRANS_GUARD_BITS((cap_)->addr_trans)
#define VG_CAP_GUARD(cap_) VG_CAP_ADDR_TRANS_GUARD((cap_)->addr_trans)

/* NB: Only updates the shadow guard; NOT the capability.  If the
   latter behavior is desired, use vg_cap_copy_x instead.  */
#define VG_CAP_SET_GUARD_SUBPAGE(cap_, guard_, gdepth_, subpage_, subpages_) \
  ({ bool r_ = true; \
     if ((subpages_) != 1 \
	 && ! ((cap_)->type == vg_cap_cappage || (cap_)->type == vg_cap_rcappage)) \
       { \
         debug (1, "Subpages are only allow for cappages."); \
         r_ = false; \
       } \
     if (r_) \
       r_ = VG_CAP_ADDR_TRANS_SET_GUARD_SUBPAGE (&(cap_)->addr_trans, \
					      (guard_), (gdepth_), \
					      (subpage_), (subpages_)); \
     r_; \
  })

#define VG_CAP_SET_GUARD(cap_, guard_, gdepth_) \
  VG_CAP_SET_GUARD_SUBPAGE ((cap_), (guard_), (gdepth_), \
			 VG_CAP_SUBPAGE ((cap_)), VG_CAP_SUBPAGES ((cap_)))
#define VG_CAP_SET_SUBPAGE(cap_, subpage_, subpages_) \
  VG_CAP_SET_GUARD_SUBPAGE ((cap_), VG_CAP_GUARD (cap_), VG_CAP_GUARD_BITS (cap_), \
			 (subpage_), (subpages_))

/* Capability-related methods.  */

#define RPC_STUB_PREFIX vg
#define RPC_ID_PREFIX VG

#include <viengoos/rpc.h>

enum
  {
    VG_cap_copy = 300,
    VG_cap_rubout,
    VG_cap_read,

    VG_object_discarded_clear = 400,
    VG_object_discard,
    VG_object_status,
    VG_object_reply_on_destruction,
    VG_object_name,
  };

enum
{
  /* Use subpage in CAP_ADDR_TRANS (must be a subset of subpage in
     SOURCE).  */
  VG_CAP_COPY_COPY_ADDR_TRANS_SUBPAGE = 1 << 0,
  /* Use guard in TARGET, not the guard in CAP_ADDR_TRANS.  */
  VG_CAP_COPY_COPY_ADDR_TRANS_GUARD = 1 << 1,
  /* Use guard in SOURCE.  */
  VG_CAP_COPY_COPY_SOURCE_GUARD = 1 << 2,

  /* When copying the capability copies a weakened reference.  */
  VG_CAP_COPY_WEAKEN = 1 << 3,

  /* Set the discardable bit on the capability.  */
  VG_CAP_COPY_DISCARDABLE_SET = 1 << 4,

  /* Set the priority of the object.  */
  VG_CAP_COPY_PRIORITY_SET = 1 << 5,
};

/* Copy the capability in capability slot SOURCE to the slot at VG_ADDR
   in the object OBJECT.  If OBJECT is VG_ADDR_VOID, then the calling
   thread's address space root is used.

   By default, preserves SOURCE's subpage specification and copies
   TARGET's guard and policy.

   If CAP_COPY_COPY_SUBPAGE is set, then uses the subpage
   specification in VG_CAP_PROPERTIES.  If VG_CAP_COPY_COPY_ADDR_TRANS_GUARD
   is set, uses the guard description in VG_CAP_PROPERTIES.

   If VG_CAP_COPY_COPY_SOURCE_GUARD is set, uses the guard description in
   source.  Otherwise, preserves the guard in TARGET.

   If VG_CAP_COPY_WEAKEN is set, saves a weakened version of SOURCE
   (e.g., if SOURCE's type is vg_cap_page, a vg_cap_rpage is saved).

   If VG_CAP_COPY_DISCARDABLE_SET is set, then sets the discardable bit
   based on the value in PROPERTIES.  Otherwise, copies SOURCE's
   value.

   If VG_CAP_COPY_PRIORITY_SET is set, then sets the priority based on
   the value in properties.  Otherwise, copies SOURCE's value.  */
RPC(cap_copy, 5, 0, 0,
    /* cap_t activity, cap_t object, */ vg_addr_t, addr,
    cap_t, source_object, vg_addr_t, source_addr,
    uintptr_t, flags, struct vg_cap_properties, properties)

/* Overwrite the capability slot at VG_ADDR in the object OBJECT with a
   void capability.  */
RPC(cap_rubout, 1, 0, 0,
    /* cap_t activity, cap_t object, */ vg_addr_t, addr)

/* Returns the public bits of the capability at address VG_ADDR in OBJECT
   in TYPE and VG_CAP_PROPERTIES.  */
RPC(cap_read, 1, 2, 0,
    /* cap_t activity, cap_t object, */ vg_addr_t, addr,
    /* Out: */
    uintptr_t, type, struct vg_cap_properties, properties)

/* Clear the discarded bit of the object at VG_ADDR in object OBJECT.  */
RPC(object_discarded_clear, 1, 0, 0,
    /* cap_t activity, cap_t object, */ vg_addr_t, addr)

/* If the object designated by OBJECT is in memory, discard it.
   OBJECT must have write authority.  This does not set the object's
   discarded bit and thus does not result in a fault.  Instead, the
   next access will see, e.g., zero-filled memory.  */
RPC(object_discard, 0, 0, 0
    /* cap_t activity, cap_t object, */)

enum
{
  object_dirty = 1 << 0,
  object_referenced = 1 << 1,
};

/* Returns whether OBJECT is dirty.  If CLEAR is set, the dirty bit is
   clear.  An object's dirty bit is set when the object is modified.
   (Note: this is not the state of a frame but an indication of
   whether the object has been modified since the last time it the
   dirty bit was cleared.)  */
RPC (object_status, 1, 1, 0,
     /* vg_addr_t activity, vg_addr_t object, */ bool, clear,
     uintptr_t, status)

/* Returns the object's return code in RETURN_CODE on object
   destruction.  */
RPC (object_reply_on_destruction, 0, 1, 0,
    /* cap_t principal, cap_t object, */
    /* Out: */
    uintptr_t, return_code);

struct object_name
{
  char name[12];
};

/* Give object OBJECT a name.  This is only used for debugging
   purposes and is only supported by some objects, in particular,
   activities and threads.  */
RPC (object_name, 1, 0, 0,
     /* cap_t activity, cap_t object, */ struct object_name, name);
     

#undef RPC_STUB_PREFIX
#undef RPC_ID_PREFIX

/* An object.  */

/* The number of capabilities per page.  */
enum
  {
    VG_CAPPAGE_SLOTS = PAGESIZE / 16,
  };
/* The log2 of the number of capabilities per page.  */
enum
  {
    VG_CAPPAGE_SLOTS_LOG2 = PAGESIZE_LOG2 - 4,
  };

struct object
{
  union
  {
    char data[PAGESIZE];
    struct vg_cap caps[VG_CAPPAGE_SLOTS];
  };
};

#ifdef RM_INTERN
typedef struct activity *activity_t;
#else
typedef vg_addr_t activity_t;
#endif

#ifndef RM_INTERN
/* Return the address of vg_cap CAP's shadow object.  */
static inline void *
vg_cap_get_shadow (const struct vg_cap *vg_cap)
{
  return vg_cap->shadow;
}

/* Set CAP's shadow object to SHADOW.  */
static inline void
vg_cap_set_shadow (struct vg_cap *vg_cap, void *shadow)
{
  vg_cap->shadow = shadow;
}
#endif

/* Given vg_cap CAP, return the corresponding object, or NULL, if there
   is none.  */
#ifdef RM_INTERN
extern struct object *vg_cap_to_object (activity_t activity, struct vg_cap *vg_cap);
#else
static inline struct object *
vg_cap_to_object (activity_t activity, struct vg_cap *vg_cap)
{
  return vg_cap_get_shadow (vg_cap);
}
#endif

/* Wrapper for the vg_cap_copy method.  Also updates shadow
   capabilities.  */
static inline bool
vg_cap_copy_x (activity_t activity,
	    vg_addr_t target_address_space, struct vg_cap *target, vg_addr_t target_addr,
	    vg_addr_t source_address_space, struct vg_cap source, vg_addr_t source_addr,
	    int flags, struct vg_cap_properties properties)
{
  /* By default, we preserve SOURCE's subpage specification.  */
  int subpage = VG_CAP_SUBPAGE (&source);
  int subpages = VG_CAP_SUBPAGES (&source);

  if ((flags & VG_CAP_COPY_COPY_ADDR_TRANS_SUBPAGE))
    /* Copy the subpage descriptor from PROPERTIES.ADDR_TRANS.  */
    {
      if (VG_CAP_ADDR_TRANS_SUBPAGES (properties.addr_trans) != 1
	  && (source.type != vg_cap_cappage
	      && source.type != vg_cap_rcappage))
	/* A subpage descriptor is only valid for
	   cappages.  */
	{
	  debug (1, "subpages (%d) specified for non-cappage "
		 "vg_cap " VG_CAP_FMT,
		 VG_CAP_ADDR_TRANS_SUBPAGES (properties.addr_trans),
		 VG_CAP_PRINTF (&source));
	  return false;
	}

      if (!
	  (/* Start of PROPERTIES.ADDR_TRANS must be at or after start of
	      SOURCE.  */
	   subpage * (256 / subpages)
	   <= (VG_CAP_ADDR_TRANS_SUBPAGE (properties.addr_trans) *
	       (256 / VG_CAP_ADDR_TRANS_SUBPAGES (properties.addr_trans)))
	   /* End of PROPERTIES.ADDR_TRANS must be before or at end of
	      SOURCE.  */
	   && (((VG_CAP_ADDR_TRANS_SUBPAGE (properties.addr_trans) + 1) *
		(256 / VG_CAP_ADDR_TRANS_SUBPAGES (properties.addr_trans)))
	       <= (subpage + 1) * (256 / subpages))))
	/* The subpage descriptor does not narrow the
	   rights.  */
	{
	  debug (1, "specified subpage (%d/%d) not a subset "
		 " of source " VG_CAP_FMT,
		 VG_CAP_ADDR_TRANS_SUBPAGE (properties.addr_trans),
		 VG_CAP_ADDR_TRANS_SUBPAGES (properties.addr_trans),
		 VG_CAP_PRINTF (&source));
	  return false;
	}

      subpage = VG_CAP_ADDR_TRANS_SUBPAGE (properties.addr_trans);
      subpages = VG_CAP_ADDR_TRANS_SUBPAGES (properties.addr_trans);
    }

  /* By default, we preserve the guard in TARGET.  */
  int guard = VG_CAP_GUARD (target);
  int gbits = VG_CAP_GUARD_BITS (target);

  if ((flags & VG_CAP_COPY_COPY_ADDR_TRANS_GUARD))
    /* Copy guard from PROPERTIES.ADDR_TRANS.  */
    {
      guard = VG_CAP_ADDR_TRANS_GUARD (properties.addr_trans);
      gbits = VG_CAP_ADDR_TRANS_GUARD_BITS (properties.addr_trans);
    }
  else if ((flags & VG_CAP_COPY_COPY_SOURCE_GUARD))
    /* Copy guard from SOURCE.  */
    {
      guard = VG_CAP_GUARD (&source);
      gbits = VG_CAP_GUARD_BITS (&source);
    }

  int type = source.type;
  if ((flags & VG_CAP_COPY_WEAKEN))
    type = vg_cap_type_weaken (type);

#ifdef RM_INTERN
  /* Changing a capability can change how addresses are translated.
     In this case, we need to shoot down all cached translations.  */
  bool changes_translation = false;

  if (target->oid != source.oid)
    {
      debug (5, "OID mismatch, changes translation");
      changes_translation = true;
    }
  else if (target->version != source.version)
    {
      debug (5, "Version mismatch, changes translation");
      changes_translation = true;
    }

  if (subpage != VG_CAP_SUBPAGE (target) || subpages != VG_CAP_SUBPAGES (target))
    {
      debug (5, "Subpage specification differs %d/%d -> %d/%d.",
	     subpage, subpages, VG_CAP_SUBPAGE (target), VG_CAP_SUBPAGES (target));
      changes_translation = true;
    }

  if (guard != VG_CAP_GUARD (target)
      || gbits != VG_CAP_GUARD_BITS (target))
    {
      debug (5, "Guard changed invalidating translation "
	     "0x%x/%d -> %llx/%d",
	     guard, gbits, VG_CAP_GUARD (target), VG_CAP_GUARD_BITS (target));
      changes_translation = true;
    }

  if (type != target->type)
    {
      debug (5, "Type changed, invalidating translation");
      changes_translation = true;
    }

  if (changes_translation)
    {
      extern void cap_shootdown (struct activity *activity, struct vg_cap *vg_cap);

      debug (5, "Translation changed: " VG_CAP_FMT " -> " VG_CAP_FMT,
	     VG_CAP_PRINTF (target), VG_CAP_PRINTF (&source));

      cap_shootdown (activity, target);
    }
#endif

  if (! VG_CAP_ADDR_TRANS_SET_GUARD_SUBPAGE (&properties.addr_trans,
					  guard, gbits,
					  subpage, subpages))
    return false;

#ifndef RM_INTERN
  assert (! VG_ADDR_IS_VOID (target_addr));
  assert (! VG_ADDR_IS_VOID (source_addr));

  error_t err = vg_cap_copy (activity, target_address_space, target_addr,
			     source_address_space, source_addr,
			     flags, properties);
  assert (err == 0);
#endif

  *target = source;
  target->addr_trans = properties.addr_trans;
  target->type = type;

  if ((flags & VG_CAP_COPY_DISCARDABLE_SET))
    target->discardable = properties.policy.discardable;

  if ((flags & VG_CAP_COPY_PRIORITY_SET))
    target->priority = properties.policy.priority;

  return true;
}

/* Copy the capability SOURCE to capability TARGET.  Preserves
   SOURCE's subpage specification and TARGET's guard.  Copies SOURCE's
   policy.  */
static inline bool
vg_cap_copy (activity_t activity,
	  vg_addr_t target_as, struct vg_cap *target, vg_addr_t target_addr,
	  vg_addr_t source_as, struct vg_cap source, vg_addr_t source_addr)
{
  return vg_cap_copy_x (activity, target_as, target, target_addr,
		     source_as, source, source_addr,
		     VG_CAP_COPY_DISCARDABLE_SET | VG_CAP_COPY_PRIORITY_SET,
		     VG_CAP_PROPERTIES_GET (source));
}

#endif
