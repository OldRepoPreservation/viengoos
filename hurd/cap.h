/* cap.h - Capability definitions.
   Copyright (C) 2007 Free Software Foundation, Inc.
   Written by Neal H. Walfield <neal@gnu.org>.

   This file is part of the GNU Hurd.

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

#ifndef _HURD_CAP_H
#define _HURD_CAP_H 1

#include <hurd/types.h>
#include <hurd/stddef.h>
#include <hurd/addr-trans.h>
#include <hurd/startup.h>
#include <errno.h>

#ifndef RM_INTERN
#include <pthread.h>

/* When reading or modifying the address space metadata, this
   lock must be held.  */
extern pthread_rwlock_t as_lock;
#endif

/* A capability.  */

#ifdef RM_INTERN
/* An OID corresponds to a page on a volume.  */
typedef l4_uint64_t oid_t;
#define OID_FMT "%llx"
#define OID_PRINTF(__op_oid) ((oid_t) (__op_oid))
#endif

#define CAP_VERSION_BITS 20
#define CAP_TYPE_BITS 6

struct cap
{
#ifdef RM_INTERN
  /* For a description of how versioning works, refer to the comment
     titled "Object versioning" in object.h.  */
  l4_uint32_t version : CAP_VERSION_BITS;

  l4_uint32_t pad0 : 32 - CAP_VERSION_BITS - CAP_TYPE_BITS;
  l4_uint32_t type : CAP_TYPE_BITS;

  struct cap_addr_trans addr_trans;

  /* If the capability names an object, the object id.  */
  oid_t oid;
#else
  /* The shadow object (only for cappages).  Maximal address is
     256G.  */
  l4_uint32_t shadow : 32 - CAP_TYPE_BITS;

  l4_uint32_t type : CAP_TYPE_BITS;

  /* This capability's address description.  */
  struct cap_addr_trans addr_trans;
#endif
};

/* Accessors corresponding to the CAP_ADDR_TRANS macros.  */
#define CAP_SUBPAGES_LOG2(cap_) \
  CAP_ADDR_TRANS_SUBPAGES_LOG2((cap_)->addr_trans)
#define CAP_SUBPAGES(cap_) CAP_ADDR_TRANS_SUBPAGES ((cap_)->addr_trans)
#define CAP_SUBPAGE(cap_) CAP_ADDR_TRANS_SUBPAGE((cap_)->addr_trans)
#define CAP_SUBPAGE_SIZE_LOG2(cap_) \
  CAP_ADDR_TRANS_SUBPAGE_SIZE_LOG2 ((cap_)->addr_trans)
#define CAP_SUBPAGE_SIZE(cap_) \
  CAP_ADDR_TRANS_SUBPAGE_SIZE ((cap_)->addr_trans)
#define CAP_SUBPAGE_OFFSET(cap_) \
  CAP_ADDR_TRANS_SUBPAGE_OFFSET((cap_)->addr_trans)
#define CAP_GUARD_BITS(cap_) CAP_ADDR_TRANS_GUARD_BITS((cap_)->addr_trans)
#define CAP_GUARD(cap_) CAP_ADDR_TRANS_GUARD((cap_)->addr_trans)

/* NB: Only updates the shadow guard; NOT the capability.  If the
   latter behavior is desired, use cap_copy_x instead.  */
#define CAP_SET_GUARD_SUBPAGE(cap_, guard_, gdepth_, subpage_, subpages_) \
  ({ bool r_ = true; \
     if ((subpages_) != 1 \
	 && ! ((cap_)->type == cap_cappage || (cap_)->type == cap_rcappage)) \
       { \
         debug (1, "Subpages are only allow for cappages."); \
         r_ = false; \
       } \
     if (r_) \
       r_ = CAP_ADDR_TRANS_SET_GUARD_SUBPAGE (&(cap_)->addr_trans, \
					      (guard_), (gdepth_), \
					      (subpage_), (subpages_)); \
     r_; \
  })

#define CAP_SET_GUARD(cap_, guard_, gdepth_) \
  CAP_SET_GUARD_SUBPAGE ((cap_), (guard_), (gdepth_), \
			 CAP_SUBPAGE ((cap_)), CAP_SUBPAGES ((cap_)))
#define CAP_SET_SUBPAGE(cap_, subpage_, subpages_) \
  CAP_SET_GUARD_SUBPAGE ((cap_), CAP_GUARD (cap_), CAP_GUARD_BITS (cap_), \
			 (subpage_), (subpages_))

/* The types of objects designated by capabilities.  */
enum cap_type
  {
#define CAP_TYPE_MIN cap_void
    cap_void,
    cap_page,
    cap_rpage,
    cap_cappage,
    cap_rcappage,
    cap_folio,
    cap_activity,
    cap_activity_control,
    cap_thread,
#define CAP_TYPE_MAX cap_thread
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
    RM_cap_copy = 300,
    RM_cap_read,

    RM_object_slot_copy_out = 400,
    RM_object_slot_copy_in,
    RM_object_slot_read,
  };

enum
{
  /* Use subpage in CAP_ADDR_TRANS (must be a subset of subpage in
     SOURCE).  */
  CAP_COPY_COPY_ADDR_TRANS_SUBPAGE = 1 << 0,
  /* Use guard in TARGET, not the guard in CAP_ADDR_TRANS.  */
  CAP_COPY_COPY_ADDR_TRANS_GUARD = 1 << 1,
  /* Use guard in SOURCE.  */
  CAP_COPY_COPY_SOURCE_GUARD = 1 << 2,

  /* When copying the capability copies a weakened reference.  */
  CAP_COPY_WEAKEN = 1 << 3
};

/* Copy capability SOURCE to the capability slot TARGET.
   ADDR_TRANS_FLAGS is a subset of CAP_COPY_GUARD, CAP_COPY_SUBPAGE,
   and CAP_COPY_PRESERVE_GUARD, bitwise-ored.  If CAP_COPY_GUARD is
   set, the guard descriptor in CAP_ADDR_TRANS is used, if
   CAP_COPY_PRESERVE_GUARD, the guard descriptor in TARGET, otherwise,
   the guard descriptor is copied from SOURCE.  If CAP_COPY_SUBPAGE is
   set, the subpage descriptor in CAP_ADDR_TRANS is used, otherwise,
   the subpage descriptor is copied from SOURCE.  */
RPC(cap_copy, 5, 0, addr_t, principal, addr_t, target, addr_t, source,
    l4_word_t, addr_trans_flags, struct cap_addr_trans, cap_addr_trans)

/* Store the public bits of the capability CAP in *TYPE and
   *CAP_ADDR_TRANS.  */
RPC(cap_read, 2, 2, addr_t, principal, addr_t, cap,
    /* Out: */
    l4_word_t, type, struct cap_addr_trans, cap_addr_trans)

/* Copy the capability from slot SLOT of the object OBJECT (relative
   to the start of the object's subpage) to slot TARGET.  */
RPC(object_slot_copy_out, 6, 0, addr_t, principal,
    addr_t, object, l4_word_t, slot, addr_t, target,
    l4_word_t, flags, struct cap_addr_trans, cap_addr_trans)

/* Copy the capability from slot SOURCE to slot INDEX of the object
   OBJECT (relative to the start of the object's subpage).  */
RPC(object_slot_copy_in, 6, 0, addr_t, principal,
    addr_t, object, l4_word_t, index, addr_t, source,
    l4_word_t, flags, struct cap_addr_trans, cap_addr_trans)

/* Store the public bits of the capability slot SLOT of object
   OBJECT in *TYPE and *CAP_ADDR.  */
RPC(object_slot_read, 3, 2, addr_t, principal,
    addr_t, object, l4_word_t, slot,
    /* Out: */
    l4_word_t, type, struct cap_addr_trans, cap_addr_trans)

#undef RPC_STUB_PREFIX
#undef RPC_ID_PREFIX
#undef RPC_TARGET

/* Return whether cap CAP is of type TYPE.  */
static inline bool
cap_is_a (struct cap *cap, enum cap_type type)
{
  return cap->type == type;
}

/* Return whether capability CAP allegedly designates a page
   object.  */
static inline bool
cap_is_a_page (struct cap *cap)
{
  return cap_is_a (cap, cap_page) || cap_is_a (cap, cap_rpage)
    || cap_is_a (cap, cap_cappage) || cap_is_a (cap, cap_rcappage);
}

/* Return whether two types are compatible in the sense that two caps
   with the given types can designate the same object.  */
static inline bool
cap_types_compatible (enum cap_type a, enum cap_type b)
{
  if (a == b)
    return true;

  if (a == cap_page && b == cap_rpage)
    return true;
  if (a == cap_rpage && b == cap_page)
    return true;

  if (a == cap_cappage && b == cap_rcappage)
    return true;
  if (a == cap_rcappage && b == cap_cappage)
    return true;

  if (a == cap_activity && b == cap_activity_control)
    return true;
  if (a == cap_activity_control && b == cap_activity)
    return true;

  return false;
}

/* Returns weather TYPE corresponds to a weak type.  */
static inline bool
cap_type_weak_p (enum cap_type type)
{
  switch (type)
    {
    case cap_rpage:
    case cap_rcappage:
    case cap_activity:
      return true;

    default:
      return false;
    }
}

/* Returns the weakened type corresponding to TYPE.  If type is
   already a weak type, returns TYPE.  */
static inline enum cap_type
cap_type_weaken (enum cap_type type)
{
  switch (type)
    {
    case cap_page:
    case cap_rpage:
      return cap_rpage;

    case cap_cappage:
    case cap_rcappage:
      return cap_rcappage;

    case cap_activity_control:
    case cap_activity:
      return cap_activity;

    default:
      return cap_void;
    }
}

static inline const char *
cap_type_string (enum cap_type type)
{
  switch (type)
    {
    case cap_void:
      return "void";
    case cap_page:
      return "page";
    case cap_rpage:
      return "rpage";
    case cap_cappage:
      return "cappage";
    case cap_rcappage:
      return "rcappage";
    case cap_folio:
      return "folio";
    case cap_activity:
      return "activity";
    case cap_activity_control:
      return "activity_control";
    case cap_thread:
      return "thread";
    default:
      return "unknown cap type";
  };
}

#ifdef RM_INTERN
#define CAP_FMT "{ %lld.%d:%s %llx/%d; %d/%d }"
#define CAP_PRINTF(cap) \
  (cap)->oid, (cap)->version, cap_type_string ((cap)->type), \
  CAP_GUARD ((cap)), CAP_GUARD_BITS ((cap)), \
  CAP_SUBPAGE ((cap)), CAP_SUBPAGES ((cap))
#else
#define CAP_FMT "{ %s %llx/%d; %d/%d }"
#define CAP_PRINTF(cap) \
  cap_type_string ((cap)->type), \
  CAP_GUARD ((cap)), CAP_GUARD_BITS ((cap)), \
  CAP_SUBPAGE ((cap)), CAP_SUBPAGES ((cap))
#endif

/* An object.  */

/* The number of capabilities per page.  */
enum
  {
    CAPPAGE_SLOTS = PAGESIZE / 16,
  };
/* The log2 of the number of capabilities per page.  */
enum
  {
    CAPPAGE_SLOTS_LOG2 = PAGESIZE_LOG2 - 4,
  };

struct object
{
  union
  {
    char data[PAGESIZE];
    struct cap caps[CAPPAGE_SLOTS];
  };
};

#ifdef RM_INTERN
typedef struct activity *activity_t;
#else
typedef addr_t activity_t;
#endif

#ifndef RM_INTERN
/* Return the address of cap CAP's shadow object.  */
static inline void *
cap_get_shadow (const struct cap *cap)
{
  return (void *) ((l4_word_t) cap->shadow << PAGESIZE_LOG2);
}

/* Set CAP's shadow object to SHADOW.  */
static inline void
cap_set_shadow (struct cap *cap, void *shadow)
{
  assert (((uintptr_t) shadow & (PAGESIZE - 1)) == 0);
  cap->shadow = (uintptr_t) shadow >> PAGESIZE_LOG2;
  assert (cap_get_shadow (cap) == shadow);
}
#endif

/* Given cap CAP, return the corresponding object, or NULL, if there
   is none.  */
#ifdef RM_INTERN
extern struct object *cap_to_object (activity_t activity,
				     struct cap *cap);
#else
static inline struct object *
cap_to_object (activity_t activity, struct cap *cap)
{
  return cap_get_shadow (cap);
}
#endif

/* Copy the capability SOURCE to capability TARGET.  By default,
   preserves SOURCE's subpage specification and TARGET's guard.  If
   CAP_COPY_COPY_SUBPAGE is set, then uses the subpage specification
   in CAP_ADDR_TRANS.  If CAP_COPY_COPY_ADDR_TRANS_GUARD is set, uses
   the guard description in CAP_ADDR_TRANS.  If
   CAP_COPY_COPY_SOURCE_GUARD is set, uses the guard description in
   source.  Otherwise, preserves the guard in TARGET.  */
static inline bool
cap_copy_x (activity_t activity,
	    struct cap *target, addr_t target_addr,
	    struct cap source, addr_t source_addr,
	    int flags, struct cap_addr_trans cap_addr_trans)
{
  /* By default, we preserve SOURCE's subpage
     specification.  */
  int subpage = CAP_SUBPAGE (&source);
  int subpages = CAP_SUBPAGES (&source);

  if ((flags & CAP_COPY_COPY_ADDR_TRANS_SUBPAGE))
    /* Copy the subpage descriptor from CAP_ADDR_TRANS.  */
    {
      if (CAP_ADDR_TRANS_SUBPAGES (cap_addr_trans) != 1
	  && (source.type != cap_cappage
	      && source.type != cap_rcappage))
	/* A subpage descriptor is only valid for
	   cappages.  */
	{
	  debug (1, "subpages (%d) specified for non-cappage "
		 "cap " CAP_FMT,
		 CAP_ADDR_TRANS_SUBPAGES (cap_addr_trans),
		 CAP_PRINTF (&source));
	  return false;
	}
		    
      if (!
	  (/* Start of CAP_ADDR_TRANS must be at or after start of
	      SOURCE.  */
	   subpage * (256 / subpages)
	   <= (CAP_ADDR_TRANS_SUBPAGE (cap_addr_trans) *
	       (256 / CAP_ADDR_TRANS_SUBPAGES (cap_addr_trans)))
	   /* End of CAP_ADDR_TRANS must be before or at end of
	      SOURCE.  */
	   && (((CAP_ADDR_TRANS_SUBPAGE (cap_addr_trans) + 1) *
		(256 / CAP_ADDR_TRANS_SUBPAGES (cap_addr_trans)))
	       <= (subpage + 1) * (256 / subpages))))
	/* The subpage descriptor does not narrow the
	   rights.  */
	{
	  debug (1, "specified subpage (%d/%d) not a subset "
		 " of source " CAP_FMT,
		 CAP_ADDR_TRANS_SUBPAGE (cap_addr_trans),
		 CAP_ADDR_TRANS_SUBPAGES (cap_addr_trans),
		 CAP_PRINTF (&source));
	  return false;
	}

      subpage = CAP_ADDR_TRANS_SUBPAGE (cap_addr_trans);
      subpages = CAP_ADDR_TRANS_SUBPAGES (cap_addr_trans);
    }

  /* By default, we preserve the guard in TARGET.  */
  int guard = CAP_GUARD (target);
  int gbits = CAP_GUARD_BITS (target);

  if ((flags & CAP_COPY_COPY_ADDR_TRANS_GUARD))
    /* Copy guard from CAP_ADDR_TRANS.  */
    {
      guard = CAP_ADDR_TRANS_GUARD (cap_addr_trans);
      gbits = CAP_ADDR_TRANS_GUARD_BITS (cap_addr_trans);
    }
  else if ((flags & CAP_COPY_COPY_SOURCE_GUARD))
    /* Copy guard from SOURCE.  */
    {
      guard = CAP_GUARD (&source);
      gbits = CAP_GUARD_BITS (&source);
    }

#ifdef RM_INTERN
  /* Changing a capability can change how addresses are translated.
     In this case, we need to shoot down all cache translation.  */
  bool changes_translation = false;

  if (target->oid != source.oid)
    {
      debug (5, "OID mismatch, changes translation");
      changes_translation = true;
    }

  if (subpage != CAP_SUBPAGE (target) || subpages != CAP_SUBPAGES (target))
    {
      debug (5, "Subpage specification differs %d/%d -> %d/%d.",
	     subpage, subpages, CAP_SUBPAGE (target), CAP_SUBPAGES (target));
      changes_translation = true;
    }

  if (guard != CAP_GUARD (target)
      || gbits != CAP_GUARD_BITS (target))
    {
      debug (5, "Guard changed invalidating translation "
	     "0x%x/%d -> %llx/%d",
	     guard, gbits, CAP_GUARD (target), CAP_GUARD_BITS (target));
      changes_translation = true;
    }

  if (target->type != source.type
      && ! ((flags & CAP_COPY_WEAKEN)
	    && cap_type_weaken (source.type) == target->type))
    {
      debug (5, "Type changed, invalidating translation");
      changes_translation = true;
    }

  if (target->type != source.type
      && ! ((flags & CAP_COPY_WEAKEN)
	    && cap_type_weaken (source.type) == target->type))
    {
      debug (5, "Type changed, invalidating translation");
      changes_translation = true;
    }

  if (changes_translation)
    {
      extern void cap_shootdown (struct activity *activity, struct cap *cap);

      cap_shootdown (activity, target);
    }
#endif

  if (! CAP_ADDR_TRANS_SET_GUARD_SUBPAGE (&cap_addr_trans,
					  guard, gbits,
					  subpage, subpages))
    return false;

#ifndef RM_INTERN
  assert (! ADDR_IS_VOID (target_addr));
  assert (! ADDR_IS_VOID (source_addr));

  error_t err = rm_cap_copy (activity, target_addr,
			     source_addr, flags,
			     cap_addr_trans);
  assert (err == 0);
#endif

  *target = source;
  target->addr_trans = cap_addr_trans;

  if ((flags & CAP_COPY_WEAKEN))
    target->type = cap_type_weaken (target->type);

  return true;
}

/* Copy the capability SOURCE to capability TARGET.  Preserves
   SOURCE's subpage specification and TARGET's guard.  */
static inline bool
cap_copy (activity_t activity,
	  struct cap *target, addr_t target_addr,
	  struct cap source, addr_t source_addr)
{
  return cap_copy_x (activity, target, target_addr,
		     source, source_addr, 0, CAP_ADDR_TRANS_VOID);
}

/* Return the value of the capability at ADDR or, if an object, the
   value of the capability designating the object, in the address
   space rooted by ROOT.

   TYPE is the required type.  If the type is incompatible
   (cap_rcappage => cap_cappage and cap_rpage => cap_page), bails.  If
   TYPE is -1, then any type is acceptable.  May cause paging.  If
   non-NULL, returns whether the slot is writable in *WRITABLE.

   This function locks (and unlocks) as_lock.  */
extern struct cap cap_lookup_rel (activity_t activity,
				  struct cap *root, addr_t addr,
				  enum cap_type type, bool *writable);

/* Return the value of the capability designating the object at ADDR
   in the address space rooted by ROOT.  Unlike cap_lookup_rel, does
   not return the capability if the address designates the slot rather
   than the object itself.

   TYPE is the required type.  If the type is incompatible
   (cap_rcappage => cap_cappage and cap_rpage => cap_page), bails.  If
   TYPE is -1, then any type is acceptable.  May cause paging.  If
   non-NULL, returns whether the slot is writable in *WRITABLE.

   This function locks (and unlocks) as_lock.  */
extern struct cap object_lookup_rel (activity_t activity,
				     struct cap *root, addr_t addr,
				     enum cap_type type, bool *writable);

/* Return the capability slot at ADDR or, if an object, the slot
   referencing the object, in the address space rooted by ROOT.

   TYPE is the required type.  If the type is incompatible
   (cap_rcappage => cap_cappage and cap_rpage => cap_page), bails.  If
   TYPE is -1, then any type is acceptable.  May cause paging.  If
   non-NULL, returns whether the slot is writable in *WRITABLE.

   This function locks (and unlocks) as_lock.  */
extern struct cap *slot_lookup_rel (activity_t activity,
				    struct cap *root, addr_t addr,
				    enum cap_type type, bool *writable);

#endif
