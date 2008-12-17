/* as.h - Address space interface.
   Copyright (C) 2007, 2008 Free Software Foundation, Inc.
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

#ifndef _HURD_AS_H
#define _HURD_AS_H

#include <viengoos/addr.h>
#include <viengoos/cap.h>
#include <hurd/exceptions.h>
#include <stdbool.h>
#include <l4/types.h>

/* The address space allocator keeps track of which addresses are
   allocated and which are available.  The allocator supports the
   efficient allocation of both arbitrary and specific addresses.
   When an address is allocated, all address dominated by that address
   are allocated as well.  */

/* Allocate COUNT contiguous subtree such that the root's depth of
   each is at least VG_ADDR_BITS - WIDTH.  If DATA_MAPPABLE is true, then
   ensures that the leaves of each subtree are mappable in the region
   accessible to data instructions.  On success returns the address of
   the first subtree.  Otherwise, returns VG_ADDR_VOID.  */
extern vg_addr_t as_alloc (int width, uint64_t count,
			bool data_mappable);

/* Like as_alloc but may be called before as_init is called.  Address
   is returned in the descriptor's object field.  The caller must fill
   in the rest.  */
extern struct hurd_object_desc *as_alloc_slow (int width);

/* Allocate the COUNT contiguous addresses strating at address ADDR.
   Returns true on success, false otherwise.  */
extern bool as_alloc_at (vg_addr_t addr, uint64_t count);

/* Free the COUNT contiguous addresses starting at VG_ADDR.  Each ADDR
   must have been previously returned by a call to as_chunk_alloc or
   as_region_alloc.  All address returned by a call to as_chunk_alloc
   or as_region_alloc need not be freed by a single call to
   as_free.  */
extern void as_free (vg_addr_t addr, uint64_t count);

/* Whether as_init has completed.  */
extern bool as_init_done;

/* Initialize the address space manager.  */
extern void as_init (void);

/* Print the allocated areas.  */
extern void as_alloced_dump (const char *prefix);

/* Shadow page table management.  */

#ifndef RM_INTERN
/* For storage_check_reserve.  */
#include <hurd/storage.h>
#include <pthread.h>

/* Ensure that using the next AMOUNT bytes of stack will not result in
   a fault.  */
static void __attribute__ ((noinline))
as_lock_ensure_stack (int amount)
{
  volatile int space[amount / sizeof (int)];

  /* XXX: The stack grows up case should reverse this loop.  (Think
     about the order of the faults and how the exception handler 1)
     special cases stack faults, and 2) uses this function when it
     inserts a page.)  */
  int i;
  for (i = sizeof (space) / sizeof (int) - 1;
       i > 0;
       i -= PAGESIZE / sizeof (int))
    space[i] = 0;
}

/* The amount of stack space that needs to be available to avoid
   faulting.  */
#define AS_STACK_SPACE (8 * PAGESIZE)

/* Address space lock.  Should hold a read lock when accessing the
   address space.  Must hold a write lock when modifying the address
   space.  Because this lock is taken when resolving a fault, while
   this lock is held (either read or write), the thread may not raise
   a fault.  */
static inline void
as_lock (void)
{
  extern pthread_rwlock_t as_rwlock;
  extern l4_thread_id_t as_rwlock_owner;

  as_lock_ensure_stack (AS_STACK_SPACE);

  storage_check_reserve (false);

  for (;;)
    {
      assert (as_rwlock_owner != l4_myself ());
      pthread_rwlock_wrlock (&as_rwlock);
      assert (as_rwlock_owner == 0);
      as_rwlock_owner = l4_myself ();

      if (! storage_have_reserve ())
	{
	  as_rwlock_owner = 0;
	  pthread_rwlock_unlock (&as_rwlock);

	  storage_check_reserve (false);
	}
      else
	break;
    }
}

static inline void
as_lock_readonly (void)
{
  extern pthread_rwlock_t as_rwlock;
  extern l4_thread_id_t as_rwlock_owner;

  as_lock_ensure_stack (AS_STACK_SPACE);

  storage_check_reserve (false);

  for (;;)
    {
      assert (as_rwlock_owner != l4_myself ());
      pthread_rwlock_rdlock (&as_rwlock);
      assert (as_rwlock_owner == 0);

      if (! storage_have_reserve ())
	{
	  pthread_rwlock_unlock (&as_rwlock);

	  storage_check_reserve (false);
	}
      else
	break;
    }
}

static inline void
as_unlock (void)
{
  extern pthread_rwlock_t as_rwlock;
  extern l4_thread_id_t as_rwlock_owner;

  if (as_rwlock_owner)
    /* Only set for a write lock.  */
    {
      assert (as_rwlock_owner == l4_myself ());
      as_rwlock_owner = 0;
    }

  pthread_rwlock_unlock (&as_rwlock);
}
#else
# define as_lock() do {} while (0)
# define as_lock_readonly() do {} while (0)
# define as_unlock() do {} while (0)
#endif


#ifndef RM_INTERN
/* Because metadata is often a resource shared among the activities
   running in a particular address space, all metadata is built from a
   single activity.  This should dominate all activities running in
   this address space to avoid priority inversion.  */
extern activity_t meta_data_activity;

/* The root of the shadow page tables.  */
extern struct vg_cap shadow_root;
#endif

#if defined (RM_INTERN) || defined (NDEBUG)
#define AS_CHECK_SHADOW(root_addr, addr, cap, code)
#define AS_CHECK_SHADOW2(__acs_root_cap, __acs_addr, __acs_cap, __acs_code)
#else
#include <hurd/trace.h>

#define AS_CHECK_SHADOW(__acs_root_addr, __acs_addr, __acs_cap,		\
			__acs_code)					\
  do									\
    {									\
      uintptr_t __acs_type = -1;					\
      struct vg_cap_properties __acs_p;					\
      error_t __acs_err;						\
									\
      __acs_err = rm_cap_read (meta_data_activity,			\
			       (__acs_root_addr), (__acs_addr),		\
			       &__acs_type, &__acs_p);			\
									\
      bool die = false;							\
      if (__acs_err)							\
	die = true;							\
      else if (__acs_type == vg_cap_void)					\
	/* The kernel's type is void.  Either the shadow has not yet	\
	   been updated or the object is dead.  */			\
	;								\
      else if (__acs_type != (__acs_cap)->type)				\
	die = true;							\
      else if (! (__acs_p.policy.priority == (__acs_cap)->priority	\
		  && (!!__acs_p.policy.discardable			\
		      == !!(__acs_cap)->discardable)))			\
	die = true;							\
      else if ((__acs_type == vg_cap_cappage || __acs_type == vg_cap_rcappage) \
	       && __acs_p.addr_trans.raw != (__acs_cap)->addr_trans.raw) \
	die = true;							\
									\
      if (die)								\
	{								\
	  debug (0,							\
		 VG_ADDR_FMT "@" VG_ADDR_FMT ": err: %d; type: %s =? %s; "	\
		 "guard: %lld/%d =? %lld/%d; subpage: %d/%d =? %d/%d; "	\
		 "priority: %d =? %d; discardable: %d =? %d",		\
		 VG_ADDR_PRINTF ((__acs_root_addr)), VG_ADDR_PRINTF ((__acs_addr)), \
		 __acs_err,						\
		 vg_cap_type_string ((__acs_cap)->type),			\
		 vg_cap_type_string (__acs_type),				\
		 VG_CAP_GUARD ((__acs_cap)), VG_CAP_GUARD_BITS ((__acs_cap)),	\
		 VG_CAP_ADDR_TRANS_GUARD (__acs_p.addr_trans),		\
		 VG_CAP_ADDR_TRANS_GUARD_BITS (__acs_p.addr_trans),	\
		 VG_CAP_SUBPAGE ((__acs_cap)), VG_CAP_SUBPAGES_LOG2 ((__acs_cap)), \
		 VG_CAP_ADDR_TRANS_SUBPAGE (__acs_p.addr_trans),		\
		 VG_CAP_ADDR_TRANS_SUBPAGES_LOG2 (__acs_p.addr_trans),	\
		 (__acs_cap)->priority, __acs_p.policy.priority,	\
		 !!(__acs_cap)->discardable, !!__acs_p.policy.discardable); \
	  {								\
	    extern struct trace_buffer as_trace;			\
	    trace_buffer_dump (&as_trace, 0);				\
	  }								\
	  { (__acs_code); }						\
	  assert (! "Shadow caps inconsistent!");			\
	}								\
    }									\
  while (0)

#define AS_CHECK_SHADOW2(__acs_root_cap, __acs_addr, __acs_cap, \
			 __acs_code)				\
  do								\
    {								\
      if ((__acs_root_cap) == &shadow_root)			\
	AS_CHECK_SHADOW(VG_ADDR_VOID, (__acs_addr), (__acs_cap),	\
			(__acs_code));				\
    }								\
  while (0)
#endif

struct as_allocate_pt_ret
{
  struct vg_cap cap;
  vg_addr_t storage;
};

/* Page table allocator used by as_build.  */
typedef struct as_allocate_pt_ret (*as_allocate_page_table_t) (vg_addr_t addr);

/* Default page table allocator.  Allocates a vg_cap_cappage and the
   accompanying shadow page table.  */
extern struct as_allocate_pt_ret as_allocate_page_table (vg_addr_t addr);


/* Build up the address space, which is root at AS_ROOT_ADDR (and
   shadowed by AS_ROOT_CAP), such that there is a capability slot at
   address VG_ADDR.  Return the shadow capability.

   If MAY_OVERWRITE is true, the function is permitted to overwrite an
   existing capability.  Otherwise, only capability slots containing a
   void capability are used.

   ALLOCATE_PAGE_TABLE is a callback function that allocates a cappage and any
   required support (e.g., a shadow page table).  The function MUST
   NOT insert the address into the address space at ADDR.  It is
   called with the AS_LOCK held.  It may not recursively invoke this
   function.  It must avoid using significant amounts of stack.

   Must be called with a write lock on AS_LOCK.  Must be called with
   8kb of stack that will not fault.  */
struct vg_cap *as_build (activity_t activity,
		      vg_addr_t as_root_addr, struct vg_cap *as_root_cap,
		      vg_addr_t addr,
		      as_allocate_page_table_t allocate_page_table,
		      bool may_overwrite);

/* PT designates a cappage or a folio.  The cappage or folio is at
   address PT_ADDR.  Index the object designed by PTE returning the
   shadow capability of the idx'th capability slot.  If the capability
   is implicit (in the case of a folio), return a fabricated
   capability in *FAKE_SLOT and return FAKE_SLOT.  Return NULL on
   failure.  */
typedef struct vg_cap *(*as_object_index_t) (activity_t activity,
					  struct vg_cap *pt,
					  vg_addr_t pt_addr, int idx,
					  struct vg_cap *fake_slot);

/* Like as_buildup, but using a custom shadow page table
   implementation.  */
struct vg_cap *as_build_custom (activity_t activity,
			     vg_addr_t as_root_addr, struct vg_cap *as_root_cap,
			     vg_addr_t addr,
			     as_allocate_page_table_t allocate_page_table,
			     as_object_index_t object_index,
			     bool may_overwrite);

/* Ensure that the slot designated by VG_ADDR in the address space rooted
   at AS_ROOT_ADDR (which is shadowed by AS_ROOT_CAP) is accessible by
   allocating any required page tables and rearranging the address
   space as necessary.  Execute CODE (with AS_LOCK held) with the
   shadow capability in SLOT.  */
#define as_ensure_full(__asef_activity_,				\
		       __asef_as_root_addr_,				\
		       __asef_as_root_cap_,				\
		       __asef_addr_,					\
		       __asef_allocate_page_table_,			\
		       __asef_code)					\
  do									\
    {									\
      activity_t __asef_activity = (__asef_activity_);			\
      vg_addr_t __asef_as_root_addr = (__asef_as_root_addr_);		\
      struct vg_cap *__asef_as_root_cap = (__asef_as_root_cap_);		\
      vg_addr_t __asef_addr = (__asef_addr_);				\
      as_allocate_page_table_t __asef_allocate_page_table		\
	= (__asef_allocate_page_table_);				\
									\
      as_lock ();							\
									\
      struct vg_cap *slot = as_build (__asef_activity,			\
				   __asef_as_root_addr,			\
				   __asef_as_root_cap,			\
				   __asef_addr,				\
				   __asef_allocate_page_table,		\
				   true);				\
      assert (slot);							\
      									\
      { (__asef_code); }						\
									\
      AS_CHECK_SHADOW (__asef_as_root_addr, __asef_addr, slot, {});	\
      as_unlock ();							\
    }									\
  while (0)

#ifndef RM_INTERN
/* Like as_ensure_full, however, assumes the main address space.
   Execute CODE with the shadow capability in SLOT.  */
#define as_ensure_use(__ase_as_addr_, __ase_code)			\
  do									\
    {									\
      assert (as_init_done);						\
									\
      vg_addr_t __ase_as_addr = (__ase_as_addr_);				\
									\
      as_ensure_full (meta_data_activity,				\
		      VG_ADDR_VOID, &shadow_root,				\
		      __ase_as_addr,					\
		      as_allocate_page_table,				\
		      (__ase_code));					\
    }									\
  while (0)

/* Like as_ensure_use, however, does not execute any code.  */
#define as_ensure(__ae_addr)				\
  as_ensure_full (meta_data_activity,			\
		  VG_ADDR_VOID, &shadow_root, __ae_addr,	\
		  as_allocate_page_table,		\
		  ({;}))
#endif

/* Copy the capability located at SOURCE_ADDR (whose corresponding
   shadow capability is SOURCE_CAP) in the address space rooted at
   SOURCE_AS to TARGET_ADDR in the address space rooted at
   TARGET_AS_ROOT_ADDR (whose corresponding shadow capability is
   TARGET_AS_ROOT_CAP).

   The slot for TARGET_ADDR is first ensured to exist (ala
   as_ensure_full).  The source capability is then copied to the
   target slot.

   ALLOCATE_PAGE_TABLE is a callback to allocate page tables and any
   accompanying shadow page tables.  See as_build for details.  */
static inline void
as_insert_full (activity_t activity,
		vg_addr_t target_as_root_addr, struct vg_cap *target_as_root_cap,
		vg_addr_t target_addr,
		vg_addr_t source_as_root_addr, 
		vg_addr_t source_addr, struct vg_cap source_cap,
		as_allocate_page_table_t allocate_page_table)
{
  AS_CHECK_SHADOW (source_as_root_addr, source_addr, &source_cap, {});

  as_ensure_full (activity,
		  target_as_root_addr, target_as_root_cap,
		  target_addr,
		  allocate_page_table,
		  ({
		    bool ret;
		    ret = vg_cap_copy (activity,
				    target_as_root_addr,
				    slot,
				    target_addr,
				    source_as_root_addr,
				    source_cap,
				    source_addr);
		    assertx (ret,
			     VG_ADDR_FMT "@" VG_ADDR_FMT
			     " <- " VG_ADDR_FMT "@" VG_ADDR_FMT " (" VG_CAP_FMT ")",
			     VG_ADDR_PRINTF (target_as_root_addr),
			     VG_ADDR_PRINTF (target_addr),
			     VG_ADDR_PRINTF (source_as_root_addr),
			     VG_ADDR_PRINTF (source_addr),
			     VG_CAP_PRINTF (&source_cap));
		  }));
}

#ifndef RM_INTERN
static inline void
as_insert (vg_addr_t target_addr, 
	   vg_addr_t source_addr, struct vg_cap source_cap)
{
  as_insert_full (meta_data_activity,
		  VG_ADDR_VOID, &shadow_root, target_addr,
		  VG_ADDR_VOID, source_addr, source_cap,
		  as_allocate_page_table);
}
#endif


#ifndef RM_INTERN
/* Variant of as_ensure_full that doesn't assume the default shadow
   page table format but calls OBJECT_INDEX to index objects.  */
extern struct vg_cap *as_ensure_full_custom
  (activity_t activity,
   vg_addr_t as, struct vg_cap *root, vg_addr_t addr,
   as_allocate_page_table_t allocate_page_table,
   as_object_index_t object_index);

/* Variant of as_insert that doesn't assume the default shadow page
   table format but calls OBJECT_INDEX to index objects.  */
extern struct vg_cap *as_insert_custom
  (activity_t activity,
   vg_addr_t target_as, struct vg_cap *t_as_cap, vg_addr_t target,
   vg_addr_t source_as, struct vg_cap c_cap, vg_addr_t source,
   as_allocate_page_table_t allocate_page_table,
   as_object_index_t object_index);
#endif

union as_lookup_ret
{
  struct vg_cap cap;
  struct vg_cap *capp;
};

enum as_lookup_mode
  {
    as_lookup_want_cap,
    as_lookup_want_slot,
    as_lookup_want_object
  };

/* Lookup the capability, slot or object at address ADDR in the
   address space rooted at AS_ROOT_CAP.

   TYPE is the required type.  If the capability does not designate
   the desired type, then failure is returned.  This can also happen
   if a strong type is requested, the capability designates that type
   but that access is down-graded by a read-only capability page.  If
   TYPE is -1, this check is skipped.

   MODE is the look up mode.  If MODE is as_lookup_want_cap, a copy of
   the capability is return in *RT on success.  If MODE is
   as_lookup_want_slot, a pointer to the shadow capability is returned
   in *RT.  If the designated capability slot is implicit (for
   instance, it designates a object), failure is returned.  In both
   cases, the capability or slot is return if the address matches the
   slot or the slot plus the slot's guard.  If MODE is
   as_lookup_want_object, the capability designating the object at
   ADDR is returned.  In this case, the designating capability's guard
   must be translated by ADDR.

   On success, whether the slot or the object is writable is returned
   in *WRITABLE.  */
extern bool as_lookup_rel (activity_t activity,
			   struct vg_cap *as_root_cap, vg_addr_t addr,
			   enum vg_cap_type type, bool *writable,
			   enum as_lookup_mode mode,
			   union as_lookup_ret *ret);


/* Lookup the slot at address ADDR in the address space rooted at
   ROOT.  On success, execute the code CODE.  Whether the slot is
   writable is saved in the local variables *WRITABLE.  The shadow
   capability SLOT is stored in SLOT.  Returns whether the slot was
   found.  */
#define as_slot_lookup_rel_use(__alru_activity_,			\
			       __alru_root_, __alru_addr_,		\
			       __alru_code)				\
  ({									\
    activity_t __alru_activity = (__alru_activity_);			\
    struct vg_cap *__alru_root = (__alru_root_);				\
    vg_addr_t __alru_addr = (__alru_addr_);				\
									\
    union as_lookup_ret __alru_ret_val;					\
									\
    as_lock ();								\
									\
    bool writable;							\
    bool __alru_ret = as_lookup_rel (__alru_activity,			\
				     __alru_root, __alru_addr, -1,	\
				     &writable, as_lookup_want_slot,	\
				     &__alru_ret_val);			\
    if (__alru_ret)							\
      {									\
	struct vg_cap *slot __attribute__ ((unused)) = __alru_ret_val.capp; \
	(__alru_code);							\
									\
	AS_CHECK_SHADOW2(__alru_root, __alru_addr, slot, {});		\
      }									\
									\
    as_unlock ();							\
									\
    __alru_ret;								\
  })

#ifndef RM_INTERN
#define as_slot_lookup_use(__alu_addr, __alu_code)			\
  ({									\
    as_slot_lookup_rel_use (meta_data_activity,				\
			    &shadow_root, (__alu_addr),			\
			    (__alu_code));				\
  })
#endif


/* Return the value of the capability at ADDR or, if an object, the
   value of the capability designating the object, in the address
   space rooted by ROOT.

   TYPE is the required type.  If the type is incompatible
   (vg_cap_rcappage => vg_cap_cappage and vg_cap_rpage => vg_cap_page), bails.  If
   TYPE is -1, then any type is acceptable.  May cause paging.  If
   non-NULL, returns whether the slot is writable in *WRITABLE.

   This function locks (and unlocks) as_lock.  */
static inline struct vg_cap
as_cap_lookup_rel (activity_t activity,
		   struct vg_cap *root, vg_addr_t addr,
		   enum vg_cap_type type, bool *writable)
{
  union as_lookup_ret ret_val;

  as_lock_readonly ();

  bool ret = as_lookup_rel (activity, root, addr, type, writable,
			    as_lookup_want_cap, &ret_val);

  if (ret)
    AS_CHECK_SHADOW2 (root, addr, &ret_val.cap, {});

  as_unlock ();

  if (! ret)
    return (struct vg_cap) { .type = vg_cap_void };

  return ret_val.cap;
}


#ifndef RM_INTERN
static inline struct vg_cap
as_cap_lookup (vg_addr_t addr, enum vg_cap_type type, bool *writable)
{
  return as_cap_lookup_rel (meta_data_activity,
			    &shadow_root, addr, -1, writable);
}
#endif


/* Return the value of the capability designating the object at ADDR
   in the address space rooted by ROOT.  Unlike cap_lookup_rel, does
   not return the capability if the address designates the slot rather
   than the object itself.

   TYPE is the required type.  If the type is incompatible
   (vg_cap_rcappage => vg_cap_cappage and vg_cap_rpage => vg_cap_page), bails.  If
   TYPE is -1, then any type is acceptable.  May cause paging.  If
   non-NULL, returns whether the object is writable in *WRITABLE.

   This function locks (and unlocks) as_lock.  */
static inline struct vg_cap
as_object_lookup_rel (activity_t activity,
		      struct vg_cap *root, vg_addr_t addr,
		      enum vg_cap_type type, bool *writable)
{
  union as_lookup_ret ret_val;

  as_lock_readonly ();

  bool ret = as_lookup_rel (activity, root, addr, type, writable,
			    as_lookup_want_object, &ret_val);

  if (ret)
    AS_CHECK_SHADOW2 (root, addr, &ret_val.cap, {});

  as_unlock ();

  if (! ret)
    return (struct vg_cap) { .type = vg_cap_void };

  return ret_val.cap;
}


#ifndef RM_INTERN
static inline struct vg_cap
as_object_lookup (vg_addr_t addr, enum vg_cap_type type, bool *writable)
{
  return as_object_lookup_rel (meta_data_activity,
			       &shadow_root, addr, -1, writable);
}
#endif

/* Print the path taken to get to the slot at address ADDRESS.  */
extern void as_dump_path_rel (activity_t activity,
			      struct vg_cap *root, vg_addr_t addr);

#ifndef RM_INTERN
static inline void
as_dump_path (vg_addr_t addr)
{
  as_dump_path_rel (meta_data_activity, &shadow_root, addr);
}
#endif
  
/* Walk the address space (without using the shadow page tables),
   depth first.  VISIT is called for each slot for which (1 <<
   reported capability type) & TYPES is non-zero.  TYPE is the
   reported type of the capability and PROPERTIES the value of its
   properties.  WRITABLE is whether the slot is writable.  If VISIT
   returns a non-zero value, the walk is aborted and that value is
   returned.  If the walk is not aborted, 0 is returned.  */
extern int as_walk (int (*visit) (vg_addr_t cap,
				  uintptr_t type,
				  struct vg_cap_properties properties,
				  bool writable,
				  void *cookie),
		    int types,
		    void *cookie);

/* AS_LOCK must not be held.  */
extern void as_dump_from (activity_t activity, struct vg_cap *root,
			  const char *prefix);

#ifndef RM_INTERN
/* Dump the address space structures.  */
static inline void
as_dump (const char *prefix)
{
  as_dump_from (meta_data_activity, &shadow_root, prefix);
}
#endif

#endif /* _HURD_AS_H  */
