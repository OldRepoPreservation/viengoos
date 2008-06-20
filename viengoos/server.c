/* server.c - Server loop implementation.
   Copyright (C) 2007, 2008 Free Software Foundation, Inc.
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

#include <l4.h>
#include <l4/pagefault.h>
#include <hurd/cap.h>
#include <hurd/stddef.h>
#include <hurd/exceptions.h>
#include <hurd/thread.h>
#include <hurd/activity.h>
#include <hurd/futex.h>
#include <hurd/trace.h>
#include <hurd/as.h>

#include "server.h"

#include "rm.h"

#include "output.h"
#include "cap.h"
#include "object.h"
#include "thread.h"
#include "activity.h"
#include "viengoos.h"

#ifndef NDEBUG

struct trace_buffer rpc_trace = TRACE_BUFFER_INIT ("rpcs", 0,
						   false, false, false);

/* Like debug but also prints the method id and saves to the trace
   buffer if level is less than or equal to 4.  */
#define DEBUG(level, format, args...)					\
  do									\
    {									\
      if (level <= 4)							\
	trace_buffer_add (&rpc_trace, "(%x %s %d) " format,		\
			  thread->tid,					\
			  l4_is_pagefault (msg_tag) ? "pagefault"	\
			  : rm_method_id_string (label), label,		\
			  ##args);					\
      debug (level, "(%x %s %d) " format,				\
	     thread->tid, l4_is_pagefault (msg_tag) ? "pagefault"	\
	     : rm_method_id_string (label), label,			\
	     ##args);							\
    }									\
  while (0)

#else
#define DEBUG(level, format, args...) do {} while (0)
#endif

void
server_loop (void)
{
  error_t err;

  int do_reply = 0;
  l4_thread_id_t to = l4_nilthread;
  l4_msg_t msg;

  bool have_lock = false;
#ifndef NDEBUG
  bool rpc_trace_just_dumped = false;
#endif

  for (;;)
    {
      if (have_lock)
	{
	  ss_mutex_unlock (&kernel_lock);
	  have_lock = false;
	}

      l4_thread_id_t from = l4_anythread;
      l4_msg_tag_t msg_tag;

#ifndef NDEBUG
      l4_time_t max_idle
	= rpc_trace_just_dumped ? L4_NEVER : l4_time_period (5000 * 1000);
#else
      l4_time_t max_idle = L4_NEVER;
#endif

      /* Only accept untyped items--no strings, no mappings.  */
      l4_accept (L4_UNTYPED_WORDS_ACCEPTOR);
      if (do_reply)
	{
	  l4_msg_load (msg);

	  msg_tag = l4_reply_wait_timeout (to, max_idle, &from);
	}
      else
	msg_tag = l4_wait_timeout (max_idle, &from);

      if (l4_ipc_failed (msg_tag))
	{
#ifndef NDEBUG
	  if ((l4_error_code () & 1) && ((l4_error_code () >> 1) & 0x7) == 1)
	    /* Receive timeout.  This means that we have not gotten
	       any message in the last few seconds.  Perhaps there is
	       a dead-lock.  Dump the rpc trace.  */
	    {
	      trace_buffer_dump (&rpc_trace, 0);
	      rpc_trace_just_dumped = true;
	    }
#endif

	  debug (0, "%s %x failed: %u", 
		 l4_error_code () & 1 ? "Receiving from" : "Sending to",
		 l4_error_code () & 1 ? from : to,
		 (l4_error_code () >> 1) & 0x7);

	  do_reply = 0;
	  continue;
	}
#ifndef NDEBUG
      else
	rpc_trace_just_dumped = false;
#endif

      l4_msg_store (msg_tag, msg);
      l4_word_t label;
      label = l4_label (msg_tag);

      /* By default we reply to the sender.  */
      to = from;
      /* Unless explicitly overridden, don't reply.  */
      do_reply = 0;

      debug (5, "%x (p: %d, %x) sent %s (%x)",
	     from, l4_ipc_propagated (msg_tag), l4_actual_sender (),
	     (l4_is_pagefault (msg_tag) ? "pagefault"
	      : rm_method_id_string (label)),
	     label);

      if (l4_version (l4_myself ()) == l4_version (from))
	/* Message from a kernel thread.  */
	panic ("Kernel thread %x (propagated: %d, actual: %x) sent %s? (%x)!",
	       from, l4_ipc_propagated (msg_tag), l4_actual_sender (),
	       (l4_is_pagefault (msg_tag) ? "pagefault"
		      : rm_method_id_string (label)),
	       label);

      ss_mutex_lock (&kernel_lock);
      have_lock = true;

      /* Find the sender.  */
      struct thread *thread = thread_lookup (from);
      assert (thread);

      /* XXX: We can't charge THREAD's activity until we have the
	 activity object, however, getting the activity object may
	 require not only a few cycles but also storage and disk
	 activity.  What we could do is have a special type of
	 activity called a charge that acts as the resource principal
	 and then once we find the real principal, we just add the
	 charges to the former to the latter.  */
      struct activity *activity
	= (struct activity *) cap_to_object (root_activity,
					     &thread->activity);
      if (! activity)
	{
	  DEBUG (1, "Caller has no assigned activity");
	  continue;
	}
      if (object_type ((struct object *) activity) != cap_activity_control)
	{
	  DEBUG (1, "Caller's activity slot contains a %s,"
		 "not an activity_control",
		 cap_type_string (object_type ((struct object *) activity)));
	  continue;
	}

      if (l4_is_pagefault (msg_tag))
	/* The label is not constant: it includes the type of fault.
	   Thus, it is difficult to incorporate it into the case
	   switch below.  */
	{
	  l4_word_t access;
	  l4_word_t ip;
	  l4_word_t fault = l4_pagefault (msg_tag, &access, &ip);
	  bool w = !! (access & L4_FPAGE_WRITABLE);
	  enum cap_type type = w ? cap_page : cap_rpage;

	  DEBUG (5, "page fault at %x.%s (ip = %x)",
		 fault, w ? "w" : "r", ip);
	  l4_word_t page_addr = fault & ~(PAGESIZE - 1);

	  bool writable;
	  struct cap cap;
	  struct object *page = NULL;

	  bool raise_fault = false;
	  bool discarded = false;

	  cap = as_object_lookup_rel (activity, &thread->aspace,
				      addr_chop (PTR_TO_ADDR (page_addr),
						 PAGESIZE_LOG2),
				      type, &writable);
	  assert (cap.type == cap_void
		  || cap.type == cap_page
		  || cap.type == cap_rpage);

	  if (! writable && cap.discardable)
	    {
	      debug (5, "Clearing discardable predicate for cap designating "
		     OID_FMT " (%s)",
		     OID_PRINTF (cap.oid), cap_type_string (cap.type));
	      cap.discardable = false;
	    }

	  page = cap_to_object_soft (activity, &cap);
	  if (! page && cap.type != cap_void)
	    /* It's not in-memory.  See if it was discarded.  If not,
	       load it using cap_to_object.  */
	    {
	      int object = (cap.oid % (FOLIO_OBJECTS + 1)) - 1;
	      oid_t foid = cap.oid - object - 1;
	      struct folio *folio
		= (struct folio *) object_find (activity, foid,
						OBJECT_POLICY_DEFAULT);
	      assert (folio);
	      assert (object_type ((struct object *) folio) == cap_folio);

	      if (folio_object_discarded (folio, object))
		{
		  debug (5, OID_FMT " (%s) was discarded",
			 OID_PRINTF (cap.oid),
			 cap_type_string (folio_object_type (folio,
								 object)));

		  assert (! folio_object_content (folio, object));

		  raise_fault = true;
		  discarded = true;

		  DEBUG (5, "Raising discarded fault at %x", page_addr);
		}
	      else
		page = cap_to_object (activity, &cap);
	    }

	  if (! page)
	    {
	      raise_fault = true;
	      DEBUG (5, "fault (ip: %x; fault: %x.%c)!",
		     ip, fault, w ? 'w' : 'r');
	    }
	  else if (w && ! writable)
	    /* Only allow permitted writes through.  */
	    {
	      raise_fault = true;
	      DEBUG (5, "access fault (ip: %x; fault: %x.%c)!",
		     ip, fault, w ? 'w' : 'r');
	    }

	  if (raise_fault)
	    {
	      DEBUG (4, "fault (ip: %x; fault: %x.%c%s)!",
		     ip, fault, w ? 'w' : 'r', discarded ? " discarded" : "");

	      l4_word_t c = _L4_XCHG_REGS_DELIVER;
	      l4_thread_id_t targ = thread->tid;
	      l4_word_t sp = 0;
	      l4_word_t dummy = 0;
	      _L4_exchange_registers (&targ, &c,
				      &sp, &dummy, &dummy, &dummy, &dummy);

	      struct exception_info info;
	      info.access = access;
	      info.type = type;
	      info.discarded = discarded;

	      l4_msg_t msg;
	      exception_fault_send_marshal (&msg, PTR_TO_ADDR (fault),
					    sp, ip, info);

	      thread_raise_exception (activity, thread, &msg);

	      continue;
	    }

	  DEBUG (4, "%s fault at %x (ip=%x), replying with %p (" OID_FMT ")",
		 w ? "Write" : "Read", fault, ip, page, OID_PRINTF (cap.oid));
	  l4_map_item_t map_item
	    = l4_map_item (l4_fpage_add_rights (l4_fpage ((uintptr_t) page,
							  PAGESIZE),
						access),
			   page_addr);

	  object_to_object_desc (page)->mapped = true;

	  /* Formulate the reply message.  */
	  l4_pagefault_reply_formulate_in (msg, &map_item);
	  do_reply = 1;
	  continue;
	}

      struct activity *principal;

  /* If ERR_ is not 0, create a message indicating an error with the
     error code ERR_.  Go to the start of the server loop.  */
#define REPLY(err_)						\
      do							\
	{							\
	  if (err_)						\
	    {							\
	      l4_msg_clear (msg);				\
	      l4_msg_put_word (msg, 0, (err_));			\
	      l4_msg_set_untyped_words (msg, 1);		\
	      do_reply = 1;					\
	    }							\
	  goto out;						\
	}							\
      while (0)

      /* Return the capability slot corresponding to address ADDR in
	 the address space rooted at ROOT.  */
      error_t SLOT_ (struct cap *root, addr_t addr, struct cap **capp)
	{
	  bool w;
	  if (! as_slot_lookup_rel_use (activity, root, addr,
					({
					  w = writable;
					  *capp = slot;
					})))
	    {
	      DEBUG (1, "No capability slot at 0x%llx/%d",
		     addr_prefix (addr), addr_depth (addr));
	      as_dump_from (activity, root, "");
	      return ENOENT;
	    }
	  if (! w)
	    {
	      DEBUG (1, "Capability slot at 0x%llx/%d not writable",
		     addr_prefix (addr), addr_depth (addr));
	      as_dump_from (activity, root, "");
	      return EPERM;
	    }

	  return 0;
      }
#define SLOT(root_, addr_)				\
      ({						\
	struct cap *SLOT_ret;				\
	error_t err = SLOT_ (root_, addr_, &SLOT_ret);	\
	if (err)					\
	  REPLY (err);					\
	SLOT_ret;					\
      })

      /* Return a cap referencing the object at address ADDR of the
	 callers capability space if it is of type TYPE (-1 = don't care).
	 Whether the object is writable is stored in *WRITABLEP_.  */
      error_t CAP_ (struct cap *root,
		    addr_t addr, int type, bool require_writable,
		    struct cap *cap)
	{
	  bool writable = true;
	  *cap = as_cap_lookup_rel (principal, root, addr,
				    type, require_writable ? &writable : NULL);
	  if (type != -1 && ! cap_types_compatible (cap->type, type))
	    {
	      DEBUG (1, "Addr 0x%llx/%d does not reference object of "
		     "type %s but %s",
		     addr_prefix (addr), addr_depth (addr),
		     cap_type_string (type), cap_type_string (cap->type));
	      as_dump_from (activity, root, "");
	      return ENOENT;
	    }

	  if (require_writable && ! writable)
	    {
	      DEBUG (1, "Addr " ADDR_FMT " not writable",
		     ADDR_PRINTF (addr));
	      return EPERM;
	    }

	  return 0;
	}
#define CAP(root_, addr_, type_, require_writable_)			\
      ({								\
	struct cap CAP_ret;						\
	error_t err = CAP_ (root_, addr_, type_, require_writable_,	\
			    &CAP_ret);					\
	if (err)							\
	  REPLY (err);							\
	CAP_ret;							\
      })

      error_t OBJECT_ (struct cap *root,
		       addr_t addr, int type, bool require_writable,
		       struct object **objectp)
	{
	  bool writable = true;
	  struct cap cap;
	  cap = as_object_lookup_rel (principal, root, addr, type,
				      require_writable ? &writable : NULL);
	  if (type != -1 && ! cap_types_compatible (cap.type, type))
	    {
	      DEBUG (4, "Addr 0x%llx/%d does not reference object of "
		     "type %s but %s",
		     addr_prefix (addr), addr_depth (addr),
		     cap_type_string (type), cap_type_string (cap.type));
	      return ENOENT;
	    }

	  if (require_writable && ! writable)
	    {
	      DEBUG (4, "Addr " ADDR_FMT " not writable",
		     ADDR_PRINTF (addr));
	      return EPERM;
	    }

	  *objectp = cap_to_object (principal, &cap);
	  if (! *objectp)
	    {
	      DEBUG (4, "Addr " ADDR_FMT " contains a dangling pointer: "
		     CAP_FMT,
		     ADDR_PRINTF (addr), CAP_PRINTF (&cap));
	      return ENOENT;
	    }

	  return 0;
	}
#define OBJECT(root_, addr_, type_, require_writable_)	\
      ({								\
	struct object *OBJECT_ret;					\
	error_t err = OBJECT_ (root_, addr_, type_, require_writable_,	\
			       &OBJECT_ret);				\
	if (err)							\
	  REPLY (err);							\
	OBJECT_ret;							\
      })

      /* Find an address space root.  If ADDR_VOID, the current
	 thread's.  Otherwise, the object identified by ROOT_ADDR_ in
	 the caller's address space.  If that is a thread object, then
	 it's address space root.  */
#define ROOT(root_addr_)						\
      ({								\
	struct cap *root_;						\
	if (ADDR_IS_VOID (root_addr_))					\
	  root_ = &thread->aspace;					\
	else								\
	  {								\
	    /* This is annoying: 1) a thread could be in a folio so we  \
	       can't directly lookup the slot, 2) we only want the      \
	       thread if it matches the guard exactly.  */		\
	    struct object *t_;						\
	    error_t err = OBJECT_ (&thread->aspace, root_addr_,		\
				   cap_thread, true, &t_);		\
	    if (! err)							\
	      root_ = &((struct thread *) t_)->aspace;			\
	    else							\
	      root_ = SLOT (&thread->aspace, root_addr_);		\
	  }								\
	DEBUG (4, "root: " CAP_FMT, CAP_PRINTF (root_));		\
									\
	root_;								\
      })

      if (label == RM_putchar)
	{
	  int chr;
	  err = rm_putchar_send_unmarshal (&msg, &chr);
	  if (! err)
	    putchar (chr);

	  /* No reply needed.  */
	  do_reply = 0;
	  continue;
	}

      do_reply = 1;

  /* Return the next word.  */
#define ARG(word_) l4_msg_word (msg, word_);

  /* Return word WORD_.  */
#if L4_WORDSIZE == 32
#define ARG64(word_) \
  ({ \
    union { l4_uint64_t raw; struct { l4_uint32_t word[2]; }; } value_; \
    value_.word[0] = ARG (word_); \
    value_.word[1] = ARG (word_ + 1); \
    value_.raw; \
  })
#define ARG64_WORDS 2
#else
#define ARG64(word_) ARG(word_)
#define ARG64_WORDS 1
#endif

#define ARG_ADDR(word_) ((addr_t) { ARG64(word_) })

      principal = activity;
      addr_t principal_addr = ARG_ADDR (0);
      struct cap principal_cap;
      if (! ADDR_IS_VOID (principal_addr))
	{
	  principal_cap = CAP (&thread->aspace,
			       principal_addr, cap_activity, false);
	  principal = (struct activity *) cap_to_object (principal,
							 &principal_cap);
	  if (! principal)
	    {
	      DEBUG (4, "Dangling pointer at " ADDR_FMT,
		     ADDR_PRINTF (principal_addr));
	      REPLY (ENOENT);
	    }
	}
      else
	{
	  principal_cap = thread->activity;
	  principal = activity;
	}

      switch (label)
	{
	case RM_folio_alloc:
	  {
	    addr_t folio_addr;
	    struct folio_policy policy;

	    err = rm_folio_alloc_send_unmarshal (&msg, &principal_addr,
						 &folio_addr, &policy);
	    if (err)
	      REPLY (err);

	    DEBUG (4, "(" ADDR_FMT ")", ADDR_PRINTF (folio_addr));

	    struct cap *folio_slot = SLOT (&thread->aspace, folio_addr);

	    struct folio *folio = folio_alloc (principal, policy);
	    if (! folio)
	      REPLY (ENOMEM);

	    bool r = cap_set (principal, folio_slot,
			      object_to_cap ((struct object *) folio));
	    assert (r);

	    rm_folio_alloc_reply_marshal (&msg);
	    break;
	  }

	case RM_folio_free:
	  {
	    addr_t folio_addr;

	    err = rm_folio_free_send_unmarshal (&msg, &principal_addr,
						&folio_addr);
	    if (err)
	      REPLY (err);

	    DEBUG (4, "(" ADDR_FMT ")", ADDR_PRINTF (folio_addr));

	    struct folio *folio = (struct folio *) OBJECT (&thread->aspace,
							   folio_addr,
							   cap_folio, true);
	    folio_free (principal, folio);

	    rm_folio_free_reply_marshal (&msg);
	    break;
	  }

	case RM_folio_object_alloc:
	  {
	    addr_t folio_addr;
	    uint32_t idx;
	    uint32_t type;
	    struct object_policy policy;
	    uintptr_t return_code;
	    addr_t object_addr;
	    addr_t object_weak_addr;

	    err = rm_folio_object_alloc_send_unmarshal (&msg, &principal_addr,
							&folio_addr, &idx,
							&type, &policy,
							&return_code,
							&object_addr,
							&object_weak_addr);
	    if (err)
	      REPLY (err);

	    DEBUG (4, "(" ADDR_FMT ", %d, %s, (%s, %d), %d, "
		   ADDR_FMT", "ADDR_FMT")",

		   ADDR_PRINTF (folio_addr), idx, cap_type_string (type),
		   policy.discardable ? "discardable" : "precious",
		   policy.priority,
		   return_code,
		   ADDR_PRINTF (object_addr),
		   ADDR_PRINTF (object_weak_addr));

	    struct folio *folio = (struct folio *) OBJECT (&thread->aspace,
							   folio_addr,
							   cap_folio, true);

	    if (idx >= FOLIO_OBJECTS)
	      REPLY (EINVAL);

	    if (! (CAP_TYPE_MIN <= type && type <= CAP_TYPE_MAX))
	      REPLY (EINVAL);

	    struct cap *object_slot = NULL;
	    if (! ADDR_IS_VOID (object_addr))
	      object_slot = SLOT (&thread->aspace, object_addr);

	    struct cap *object_weak_slot = NULL;
	    if (! ADDR_IS_VOID (object_weak_addr))
	      object_weak_slot = SLOT (&thread->aspace, object_weak_addr);

	    struct cap cap;
	    cap = folio_object_alloc (principal,
				      folio, idx, type, policy, return_code);

	    if (type != cap_void)
	      {
		if (object_slot)
		  {
		    bool r = cap_set (principal, object_slot, cap);
		    assert (r);
		  }
		if (object_weak_slot)
		  {
		    bool r = cap_set (principal, object_weak_slot, cap);
		    assert (r);
		    object_weak_slot->type
		      = cap_type_weaken (object_weak_slot->type);
		  }
	      }

	    rm_folio_object_alloc_reply_marshal (&msg);
	    break;
	  }

	case RM_folio_policy:
	  {
	    addr_t folio_addr;
	    l4_word_t flags;
	    struct folio_policy in, out;

	    err = rm_folio_policy_send_unmarshal (&msg, &principal_addr,
						  &folio_addr,
						  &flags, &in);
	    if (err)
	      REPLY (err);

	    DEBUG (4, "(" ADDR_FMT ", %d)",
		   ADDR_PRINTF (folio_addr), flags);

	    struct folio *folio = (struct folio *) OBJECT (&thread->aspace,
							   folio_addr,
							   cap_folio, true);

	    folio_policy (principal, folio, flags, in, &out);

	    rm_folio_policy_reply_marshal (&msg, out);
	    break;
	  }

	case RM_object_slot_copy_out:
	  {
	    addr_t source_as_addr;
	    addr_t source_addr;
	    struct cap source;
	    addr_t target_as_addr;
	    addr_t target_addr;
	    struct cap *target;
	    uint32_t idx;
	    uint32_t flags;
	    struct cap_properties properties;

	    struct cap object_cap;
	    struct object *object;

	    err = rm_object_slot_copy_out_send_unmarshal
	      (&msg, &principal_addr, &source_as_addr, &source_addr, &idx,
	       &target_as_addr, &target_addr, &flags, &properties);
	    if (err)
	      REPLY (err);

	    DEBUG (4, "(" ADDR_FMT "@" ADDR_FMT "+%d -> "
		   ADDR_FMT "@" ADDR_FMT ", %s%s%s%s%s%s, %s/%d %lld/%d %d/%d",

		   ADDR_PRINTF (source_as_addr), ADDR_PRINTF (source_addr),
		   idx,
		   ADDR_PRINTF (target_as_addr), ADDR_PRINTF (target_addr),
		   
		   CAP_COPY_COPY_ADDR_TRANS_SUBPAGE & flags
		   ? "copy subpage/" : "",
		   CAP_COPY_COPY_ADDR_TRANS_GUARD & flags
		   ? "copy trans guard/" : "",
		   CAP_COPY_COPY_SOURCE_GUARD & flags
		   ? "copy src guard/" : "",
		   CAP_COPY_WEAKEN & flags ? "weak/" : "",
		   CAP_COPY_DISCARDABLE_SET & flags ? "discardable/" : "",
		   CAP_COPY_PRIORITY_SET & flags ? "priority" : "",

		   properties.policy.discardable ? "discardable" : "precious",
		   properties.policy.priority,
		   CAP_ADDR_TRANS_GUARD (properties.addr_trans),
		   CAP_ADDR_TRANS_GUARD_BITS (properties.addr_trans),
		   CAP_ADDR_TRANS_SUBPAGE (properties.addr_trans),
		   CAP_ADDR_TRANS_SUBPAGES (properties.addr_trans));

	    struct cap *root = ROOT (source_as_addr);
	    object_cap = CAP (root, source_addr, -1, false);

	    root = ROOT (target_as_addr);

	    goto get_slot;

	  case RM_object_slot_copy_in:
	    err = rm_object_slot_copy_in_send_unmarshal
	      (&msg, &principal_addr, &target_as_addr, &target_addr, &idx,
	       &source_as_addr, &source_addr, &flags, &properties);
	    if (err)
	      REPLY (err);

	    DEBUG (4, "(" ADDR_FMT "@" ADDR_FMT "+%d <- "
		   ADDR_FMT "@" ADDR_FMT ", %s%s%s%s%s%s, %s/%d %lld/%d %d/%d",

		   ADDR_PRINTF (target_as_addr), ADDR_PRINTF (target_addr),
		   idx,
		   ADDR_PRINTF (source_as_addr), ADDR_PRINTF (source_addr),
		   
		   CAP_COPY_COPY_ADDR_TRANS_SUBPAGE & flags
		   ? "copy subpage/" : "",
		   CAP_COPY_COPY_ADDR_TRANS_GUARD & flags
		   ? "copy trans guard/" : "",
		   CAP_COPY_COPY_SOURCE_GUARD & flags
		   ? "copy src guard/" : "",
		   CAP_COPY_WEAKEN & flags ? "weak/" : "",
		   CAP_COPY_DISCARDABLE_SET & flags ? "discardable/" : "",
		   CAP_COPY_PRIORITY_SET & flags ? "priority" : "",

		   properties.policy.discardable ? "discardable" : "precious",
		   properties.policy.priority,
		   CAP_ADDR_TRANS_GUARD (properties.addr_trans),
		   CAP_ADDR_TRANS_GUARD_BITS (properties.addr_trans),
		   CAP_ADDR_TRANS_SUBPAGE (properties.addr_trans),
		   CAP_ADDR_TRANS_SUBPAGES (properties.addr_trans));

	    root = ROOT (target_as_addr);
	    object_cap = CAP (root, target_addr, -1, true);

	    root = ROOT (source_as_addr);

	  get_slot:
	    if (idx >= cap_type_num_slots[object_cap.type])
	      REPLY (EINVAL);

	    if (object_cap.type == cap_cappage
		|| object_cap.type == cap_rcappage)
	      /* Ensure that IDX falls within the subpage.  */
	      {
		if (idx >= CAP_SUBPAGE_SIZE (&object_cap))
		  {
		    DEBUG (1, "index (%d) >= subpage size (%d)",
			   idx, CAP_SUBPAGE_SIZE (&object_cap));
		    REPLY (EINVAL);
		  }

		idx += CAP_SUBPAGE_OFFSET (&object_cap);
	      }

	    object = cap_to_object (principal, &object_cap);
	    if (! object)
	      {
		DEBUG (1, CAP_FMT " maps to void", CAP_PRINTF (&object_cap));
		REPLY (EINVAL);
	      }

	    if (label == RM_object_slot_copy_out)
	      {
		source = ((struct cap *) object)[idx];
		target = SLOT (root, target_addr);
	      }
	    else
	      {
		source = CAP (root, source_addr, -1, false);
		target = &((struct cap *) object)[idx];
	      }

	    goto cap_copy_body;

	  case RM_cap_copy:
	    err = rm_cap_copy_send_unmarshal (&msg,
					      &principal_addr,
					      &target_as_addr, &target_addr,
					      &source_as_addr, &source_addr,
					      &flags, &properties);
	    if (err)
	      REPLY (err);

	    DEBUG (4, "(" ADDR_FMT "@" ADDR_FMT " <- "
		   ADDR_FMT "@" ADDR_FMT ", %s%s%s%s%s%s, %s/%d %lld/%d %d/%d",

		   ADDR_PRINTF (target_as_addr), ADDR_PRINTF (target_addr),
		   ADDR_PRINTF (source_as_addr), ADDR_PRINTF (source_addr),
		   
		   CAP_COPY_COPY_ADDR_TRANS_SUBPAGE & flags
		   ? "copy subpage/" : "",
		   CAP_COPY_COPY_ADDR_TRANS_GUARD & flags
		   ? "copy trans guard/" : "",
		   CAP_COPY_COPY_SOURCE_GUARD & flags
		   ? "copy src guard/" : "",
		   CAP_COPY_WEAKEN & flags ? "weak/" : "",
		   CAP_COPY_DISCARDABLE_SET & flags ? "discardable/" : "",
		   CAP_COPY_PRIORITY_SET & flags ? "priority" : "",

		   properties.policy.discardable ? "discardable" : "precious",
		   properties.policy.priority,
		   CAP_ADDR_TRANS_GUARD (properties.addr_trans),
		   CAP_ADDR_TRANS_GUARD_BITS (properties.addr_trans),
		   CAP_ADDR_TRANS_SUBPAGE (properties.addr_trans),
		   CAP_ADDR_TRANS_SUBPAGES (properties.addr_trans));

	    root = ROOT (target_as_addr);
	    target = SLOT (root, target_addr);

	    root = ROOT (source_as_addr);
	    source = CAP (root, source_addr, -1, false);

	  cap_copy_body:;

	    if ((flags & ~(CAP_COPY_COPY_ADDR_TRANS_SUBPAGE
			   | CAP_COPY_COPY_ADDR_TRANS_GUARD
			   | CAP_COPY_COPY_SOURCE_GUARD
			   | CAP_COPY_WEAKEN
			   | CAP_COPY_DISCARDABLE_SET
			   | CAP_COPY_PRIORITY_SET)))
	      REPLY (EINVAL);

	    DEBUG (4, "(target: (" ADDR_FMT ") " ADDR_FMT ", "
		   "source: (" ADDR_FMT ") " ADDR_FMT ", "
		   "%s|%s, %s {%llx/%d %d/%d})",
		   ADDR_PRINTF (target_as_addr), ADDR_PRINTF (target_addr),
		   ADDR_PRINTF (source_as_addr), ADDR_PRINTF (source_addr),
		   flags & CAP_COPY_COPY_ADDR_TRANS_GUARD ? "copy trans"
		   : (flags & CAP_COPY_COPY_SOURCE_GUARD ? "source"
		      : "preserve"),
		   flags & CAP_COPY_COPY_ADDR_TRANS_SUBPAGE ? "copy"
		   : "preserve",
		   flags & CAP_COPY_WEAKEN ? "weaken" : "no weaken",
		   CAP_ADDR_TRANS_GUARD (properties.addr_trans),
		   CAP_ADDR_TRANS_GUARD_BITS (properties.addr_trans),
		   CAP_ADDR_TRANS_SUBPAGE (properties.addr_trans),
		   CAP_ADDR_TRANS_SUBPAGES (properties.addr_trans));

	    bool r = cap_copy_x (principal,
				 ADDR_VOID, target, ADDR_VOID,
				 ADDR_VOID, source, ADDR_VOID,
				 flags, properties);
	    if (! r)
	      REPLY (EINVAL);

	    if ((flags & (CAP_COPY_DISCARDABLE_SET | CAP_COPY_PRIORITY_SET)))
	      /* The caller changed the policy.  Also change it on the
		 object.  */
	      {
		struct object *object = cap_to_object_soft (principal, target);
		if (object)
		  {
		    struct object_desc *desc = object_to_object_desc (object);

		    /* XXX: This should only be allowed if TARGET
		       grants writable access to the object.  */
		    if ((flags & CAP_COPY_DISCARDABLE_SET))
		      desc->policy.discardable = properties.policy.discardable;

		    if ((flags & CAP_COPY_PRIORITY_SET)
			&& desc->activity == principal)
		      desc->policy.priority = properties.policy.priority;
		  }
	      }

	    switch (label)
	      {
	      case RM_object_slot_copy_out:
		rm_object_slot_copy_out_reply_marshal (&msg);
		break;
	      case RM_object_slot_copy_in:
		rm_object_slot_copy_in_reply_marshal (&msg);
		break;
	      case RM_cap_copy:
		rm_cap_copy_reply_marshal (&msg);
		break;
	      }
	    break;
	  }

	case RM_cap_rubout:
	  {
	    addr_t target_as_addr;
	    addr_t target_addr;

	    err = rm_cap_rubout_send_unmarshal (&msg,
						&principal_addr,
						&target_as_addr,
						&target_addr);
	    if (err)
	      REPLY (err);

	    DEBUG (4, ADDR_FMT "@" ADDR_FMT,
		   ADDR_PRINTF (target_as_addr),
		   ADDR_PRINTF (target_addr));

	    struct cap *root = ROOT (target_as_addr);

	    /* We don't look up the argument directly as we need to
	       respect any subpag specification for cappages.  */
	    struct cap *target = SLOT (root, target_addr);

	    target->type = cap_void;

	    rm_cap_rubout_reply_marshal (&msg);
	    break;
	  }

	case RM_object_slot_read:
	  {
	    addr_t root_addr;
	    addr_t source_addr;
	    uint32_t idx;

	    err = rm_object_slot_read_send_unmarshal (&msg,
						      &principal_addr,
						      &root_addr,
						      &source_addr, &idx);
	    if (err)
	      REPLY (err);

	    DEBUG (4, ADDR_FMT "@" ADDR_FMT "+%d",
		   ADDR_PRINTF (root_addr), ADDR_PRINTF (source_addr), idx);

	    struct cap *root = ROOT (root_addr);

	    /* We don't look up the argument directly as we need to
	       respect any subpag specification for cappages.  */
	    struct cap source = CAP (root, source_addr, -1, false);

	    struct object *object = cap_to_object (principal, &source);
	    if (! object)
	      REPLY (EINVAL);

	    if (idx >= cap_type_num_slots[source.type])
	      REPLY (EINVAL);

	    if (source.type == cap_cappage || source.type == cap_rcappage)
	      /* Ensure that idx falls within the subpage.  */
	      {
		if (idx >= CAP_SUBPAGE_SIZE (&source))
		  REPLY (EINVAL);

		idx += CAP_SUBPAGE_OFFSET (&source);
	      }

	    source = ((struct cap *) object)[idx];

	    rm_object_slot_read_reply_marshal (&msg, source.type,
					       CAP_PROPERTIES_GET (source));
	    break;
	  }

	case RM_cap_read:
	  {
	    addr_t root_addr;
	    addr_t source_addr;

	    err = rm_cap_read_send_unmarshal (&msg, &principal_addr,
					      &root_addr,
					      &source_addr);
	    if (err)
	      REPLY (err);

	    DEBUG (4, ADDR_FMT "@" ADDR_FMT,
		   ADDR_PRINTF (root_addr), ADDR_PRINTF (source_addr));

	    struct cap *root = ROOT (root_addr);

	    struct cap source = CAP (root, source_addr, -1, false);

	    rm_cap_read_reply_marshal (&msg, source.type,
				       CAP_PROPERTIES_GET (source));
	    break;
	  }

	case RM_object_discarded_clear:
	  {
	    addr_t object_addr;

	    err = rm_object_discarded_clear_send_unmarshal
	      (&msg, &principal_addr, &object_addr);
	    if (err)
	      REPLY (err);

	    DEBUG (4, ADDR_FMT, ADDR_PRINTF (object_addr));

	    /* We can't look up the object here as object_lookup
	       returns NULL if the object's discardable bit is
	       set!  Instead, we lookup the capability.  */
	    struct cap cap = CAP (&thread->aspace, object_addr, -1, true);

	    int idx = (cap.oid % (1 + FOLIO_OBJECTS)) - 1;
	    oid_t foid = cap.oid - idx - 1;

	    struct folio *folio = (struct folio *)
	      object_find (activity, foid, OBJECT_POLICY_VOID);

	    if (folio_object_version (folio, idx) != cap.version)
	      REPLY (ENOENT);

	    folio_object_discarded_set (folio, idx, false);

	    rm_object_discarded_clear_reply_marshal (&msg);
	    break;
	  }

	case RM_object_status:
	  {
	    addr_t object_addr;
	    bool clear;

	    err = rm_object_status_send_unmarshal
	      (&msg, &principal_addr, &object_addr, &clear);
	    if (err)
	      REPLY (err);

	    DEBUG (4, ADDR_FMT ", %sclear",
		   ADDR_PRINTF (object_addr), clear ? "" : "no ");

	    struct object *object = OBJECT (&thread->aspace,
					    object_addr, -1, true);

	    struct object_desc *desc = object_to_object_desc (object);
	    uintptr_t status = (desc->user_referenced ? object_referenced : 0)
	      | (desc->user_dirty ? object_dirty : 0);

	    if (clear)
	      {
		desc->user_referenced = 0;
		desc->user_dirty = 0;
	      }

	    rm_object_status_reply_marshal (&msg, status);
	    break;
	  }

	case RM_thread_exregs:
	  {
	    struct hurd_thread_exregs_in in;
	    addr_t target;
	    l4_word_t control;
	    err = rm_thread_exregs_send_unmarshal (&msg,
						   &principal_addr, &target,
						   &control, &in);
	    if (err)
	      REPLY (err);

	    DEBUG (4, ADDR_FMT, ADDR_PRINTF (target));

	    struct thread *t
	      = (struct thread *) OBJECT (&thread->aspace,
					  target, cap_thread, true);

	    struct cap *aspace = NULL;
	    struct cap aspace_cap;
	    if ((HURD_EXREGS_SET_ASPACE & control))
	      {
		aspace_cap = CAP (&thread->aspace, in.aspace, -1, false);
		aspace = &aspace_cap;
	      }

	    struct cap *a = NULL;
	    struct cap a_cap;
	    if ((HURD_EXREGS_SET_ACTIVITY & control))
	      {
		if (ADDR_IS_VOID (in.activity))
		  a = &thread->activity;
		else
		  {
		    a_cap = CAP (&thread->aspace,
				 in.activity, cap_activity, false);
		    a = &a_cap;
		  }
	      }

	    struct cap *exception_page = NULL;
	    struct cap exception_page_cap;
	    if ((HURD_EXREGS_SET_EXCEPTION_PAGE & control))
	      {
		exception_page_cap = CAP (&thread->aspace,
					  in.exception_page, cap_page, true);
		exception_page = &exception_page_cap;
	      }

	    struct cap *aspace_out = NULL;
	    if ((HURD_EXREGS_GET_REGS & control)
		&& ! ADDR_IS_VOID (in.aspace_out))
	      aspace_out = SLOT (&thread->aspace, in.aspace_out);

	    struct cap *activity_out = NULL;
	    if ((HURD_EXREGS_GET_REGS & control)
		&& ! ADDR_IS_VOID (in.activity_out))
	      activity_out = SLOT (&thread->aspace, in.activity_out);

	    struct cap *exception_page_out = NULL;
	    if ((HURD_EXREGS_GET_REGS & control)
		&& ! ADDR_IS_VOID (in.exception_page_out))
	      exception_page_out = SLOT (&thread->aspace,
					 in.exception_page_out);

	    struct hurd_thread_exregs_out out;
	    out.sp = in.sp;
	    out.ip = in.ip;
	    out.eflags = in.eflags;
	    out.user_handle = in.user_handle;

	    err = thread_exregs (principal, t, control,
				 aspace, in.aspace_cap_properties_flags,
				 in.aspace_cap_properties, a, exception_page,
				 &out.sp, &out.ip,
				 &out.eflags, &out.user_handle,
				 aspace_out, activity_out,
				 exception_page_out);
	    if (err)
	      REPLY (err);

	    rm_thread_exregs_reply_marshal (&msg, out);

	    break;
	  }

	case RM_thread_wait_object_destroyed:
	  {
	    addr_t addr;
	    err = rm_thread_wait_object_destroyed_send_unmarshal
	      (&msg, &principal_addr, &addr);
	    if (err)
	      REPLY (err);

	    DEBUG (4, ADDR_FMT, ADDR_PRINTF (addr));

	    struct object *object = OBJECT (&thread->aspace, addr, -1, true);

	    thread->wait_reason = THREAD_WAIT_DESTROY;
	    object_wait_queue_enqueue (principal, object, thread);

	    do_reply = 0;
	    break;
	  }

	case RM_activity_policy:
	  {
	    uintptr_t flags;
	    struct activity_policy in;

	    err = rm_activity_policy_send_unmarshal (&msg, &principal_addr,
						     &flags, &in);
	    if (err)
	      REPLY (err);

	    DEBUG (4, "");

	    if (principal_cap.type != cap_activity_control
		&& (flags & (ACTIVITY_POLICY_STORAGE_SET
			     | ACTIVITY_POLICY_CHILD_REL_SET)))
	      REPLY (EPERM);

	    rm_activity_policy_reply_marshal (&msg, principal->policy);

	    if ((flags & (ACTIVITY_POLICY_CHILD_REL_PRIORITY_SET
			  | ACTIVITY_POLICY_CHILD_REL_WEIGHT_SET
			  | ACTIVITY_POLICY_SIBLING_REL_PRIORITY_SET
			  | ACTIVITY_POLICY_SIBLING_REL_WEIGHT_SET
			  | ACTIVITY_POLICY_STORAGE_SET)))
	      {
		struct activity_policy p = principal->policy;

		if ((flags & ACTIVITY_POLICY_CHILD_REL_PRIORITY_SET))
		  p.child_rel.priority = in.child_rel.priority;
		if ((flags & ACTIVITY_POLICY_CHILD_REL_WEIGHT_SET))
		  p.child_rel.weight = in.child_rel.weight;

		if ((flags & ACTIVITY_POLICY_SIBLING_REL_PRIORITY_SET))
		  p.sibling_rel.priority = in.sibling_rel.priority;
		if ((flags & ACTIVITY_POLICY_SIBLING_REL_WEIGHT_SET))
		  p.sibling_rel.weight = in.sibling_rel.weight;

		if ((flags & ACTIVITY_POLICY_STORAGE_SET))
		  p.folios = in.folios;

		activity_policy_update (principal, p);
	      }

	    break;
	  }

	case RM_activity_stats:
	  {
	    uintptr_t until_period;
	    err = rm_activity_stats_send_unmarshal (&msg, &principal_addr,
						    &until_period);
	    if (err)
	      REPLY (err);

	    DEBUG (4, "%d", until_period);

	    int period = principal->current_period - 1;
	    if (period < 0)
	      period = (ACTIVITY_STATS_PERIODS + 1) + period;

	    if (principal->stats[period].period < until_period)
	      /* Queue thread on the activity.  */
	      {
		thread->wait_reason = THREAD_WAIT_STATS;
		thread->wait_reason_arg = until_period;

		object_wait_queue_enqueue (principal,
					   (struct object *) principal,
					   thread);

		do_reply = 0;
	      }
	    else
	      /* Return the available statistics.  */
	      {
		/* XXX: Only return valid stat buffers.  */
		struct activity_stats_buffer buffer;
		int i;
		for (i = 0; i < ACTIVITY_STATS_PERIODS; i ++)
		  {
		    period = principal->current_period - 1 - i;
		    if (period < 0)
		      period = (ACTIVITY_STATS_PERIODS + 1) + period;

		    buffer.stats[i] = principal->stats[period];
		  }

		rm_activity_stats_reply_marshal (&msg,
						 buffer,
						 ACTIVITY_STATS_PERIODS);
	      }

	    break;
	  }

	case RM_exception_collect:
	  {
	    /* We don't expect a principal.  */
	    err = rm_exception_collect_send_unmarshal (&msg, &principal_addr);
	    if (err)
	      REPLY (err);

	    panic ("Collecting exception: %x.%x", from);
#warning exception_collect not implemented

	    /* XXX: Implement me.  */

	    break;
	  }

	case RM_as_dump:
	  {
	    addr_t root_addr;
	    err = rm_as_dump_send_unmarshal (&msg, &principal_addr,
					     &root_addr);
	    if (err)
	      REPLY (err);

	    DEBUG (4, "");

	    struct cap *root = ROOT (root_addr);

	    as_dump_from (principal, root, "");

	    rm_as_dump_reply_marshal (&msg);

	    break;
	  }

	case RM_futex:
	  {
	    /* Helper function to wake and requeue waiters.  */
	    int wake (int to_wake, struct object *object1, int offset1,
		      int to_requeue, struct object *object2, int offset2)
	    {
	      int count = 0;
	      struct thread *t;

	      object_wait_queue_for_each (principal, object1, t)
		if (t->wait_reason == THREAD_WAIT_FUTEX
		    && t->wait_reason_arg == offset1)
		  /* Got a match.  */
		  {
		    if (count < to_wake)
		      {
			object_wait_queue_dequeue (principal, t);

			debug (5, "Waking thread %x", t->tid);

			err = rm_futex_reply (t->tid, 0);
			if (err)
			  panic ("Error futex waking %x: %d", t->tid, err);

			count ++;

			if (count == to_wake && to_requeue == 0)
			  break;
		      }
		    else
		      {
			object_wait_queue_dequeue (principal, t);

			t->wait_reason_arg = offset2;
			object_wait_queue_enqueue (principal, object2, t);

			count ++;

			to_requeue --;
			if (to_requeue == 0)
			  break;
		      }
		  }
	      return count;
	    }

	    void *addr1;
	    int op;
	    int val1;
	    bool timeout;
	    union futex_val2 val2;
	    void *addr2;
	    union futex_val3 val3;

	    err = rm_futex_send_unmarshal (&msg, &principal_addr,
					   &addr1, &op, &val1,
					   &timeout, &val2,
					   &addr2, &val3);
	    if (err)
	      REPLY (err);

	    {
	      const char *op_string = "unknown";
	      switch (op)
		{
		case FUTEX_WAIT:
		  op_string = "wait";
		  break;
		case FUTEX_WAKE_OP:
		  op_string = "wake op";
		  break;
		case FUTEX_WAKE:
		  op_string = "wake";
		  break;
		case FUTEX_CMP_REQUEUE:
		  op_string = "cmp requeue";
		  break;
		}

	      char *mode = "unknown";

	      struct object *page = cap_to_object (principal,
						   &thread->exception_page);
	      if (page && object_type (page) == cap_page)
		{
		  struct exception_page *exception_page
		    = (struct exception_page *) page;

		  if (exception_page->activated_mode)
		    mode = "activated";
		  else
		    mode = "normal";
		}

	      DEBUG (4, "(%p, %s, 0x%x, %d, %p, %d) (thread in %s)",
		     addr1, op_string, val1, val2.value, addr2, val3.value,
		     mode);
	    }

	    switch (op)
	      {
	      case FUTEX_WAIT:
	      case FUTEX_WAKE_OP:
	      case FUTEX_WAKE:
	      case FUTEX_CMP_REQUEUE:
		break;
	      default:
		REPLY (ENOSYS);
	      };

	    addr_t addr = addr_chop (PTR_TO_ADDR (addr1), PAGESIZE_LOG2);
	    struct object *object1 = OBJECT (&thread->aspace,
					     addr, cap_page, true);
	    int offset1 = (uintptr_t) addr1 & (PAGESIZE - 1);
	    int *vaddr1 = (void *) object1 + offset1;

	    switch (op)
	      {
	      case FUTEX_WAIT:
		if (*vaddr1 != val1)
		  REPLY (EWOULDBLOCK);

		if (timeout)
		  panic ("Timeouts not yet supported");

		thread->wait_reason = THREAD_WAIT_FUTEX;
		thread->wait_reason_arg = offset1;

		object_wait_queue_enqueue (principal, object1, thread);

		/* Don't reply.  */
		do_reply = 0;
		break;

	      case FUTEX_WAKE:
		/* Wake up VAL1 threads or, if there are less than
		   VAL1 blocked threads, wake up all of them.  Return
		   the number of threads woken up.  */

		if (val1 <= 0)
		  REPLY (EINVAL);

		int count = wake (val1, object1, offset1, 0, 0, 0);
		rm_futex_reply_marshal (&msg, count);
		break;

	      case FUTEX_WAKE_OP:
		addr = addr_chop (PTR_TO_ADDR (addr2), PAGESIZE_LOG2);
		struct object *object2 = OBJECT (&thread->aspace,
						 addr, cap_page, true);
		int offset2 = (uintptr_t) addr2 & (PAGESIZE - 1);
		int *vaddr2 = (void *) object2 + offset2;

		int oldval = * (int *) vaddr2;
		switch (val3.op)
		  {
		  case FUTEX_OP_SET:
		    * (int *) vaddr2 = val3.oparg;
		    break;
		  case FUTEX_OP_ADD:
		    * (int *) vaddr2 = oldval + val3.oparg;
		    break;
		  case FUTEX_OP_OR:
		    * (int *) vaddr2 = oldval + val3.oparg;
		    break;
		  case FUTEX_OP_ANDN:
		    * (int *) vaddr2 = oldval & ~val3.oparg;
		    break;
		  case FUTEX_OP_XOR:
		    * (int *) vaddr2 = oldval ^ val3.oparg;
		    break;
		  }

		count = wake (1, object1, offset1, 0, 0, 0);

		bool comparison;
		switch (val3.cmp)
		  {
		  case FUTEX_OP_CMP_EQ:
		    comparison = oldval == val3.cmparg;
		    break;
		  case FUTEX_OP_CMP_NE:
		    comparison = oldval != val3.cmparg;
		    break;
		  case FUTEX_OP_CMP_LT:
		    comparison = oldval < val3.cmparg;
		    break;
		  case FUTEX_OP_CMP_LE:
		    comparison = oldval <= val3.cmparg;
		    break;
		  case FUTEX_OP_CMP_GT:
		    comparison = oldval > val3.cmparg;
		    break;
		  case FUTEX_OP_CMP_GE:
		    comparison = oldval >= val3.cmparg;
		    break;
		  }

		if (comparison)
		  count += wake (val2.value, object2, offset2, 0, 0, 0);

		rm_futex_reply_marshal (&msg, 0);
		break;

	      case FUTEX_CMP_REQUEUE:
		/* Wake VAL1 waiters, or if there are less than VAL1
		   waiters, all waiters.  If there waiters remain,
		   requeue VAL2 waiters on ADDR2 or, if there are less
		   than VAL2 remaining waiters.  Returns the total
		   number of woken and requeued waiters.  */

		if (* (int *) vaddr1 != val3.value)
		  REPLY (EAGAIN);

		/* Get the second object.  */
		addr = addr_chop (PTR_TO_ADDR (addr2), PAGESIZE_LOG2);
		object2 = OBJECT (&thread->aspace, addr, cap_page, true);
		offset2 = (uintptr_t) addr2 & (PAGESIZE - 1);

		count = wake (val1, object1, offset1,
			      val2.value, object2, offset2);
		rm_futex_reply_marshal (&msg, count);
		break;
	      }

	    break;
	  }

	default:
	  /* XXX: Don't panic when running production code.  */
	  DEBUG (1, "Didn't handle message from %x.%x with label %d",
		 l4_thread_no (from), l4_version (from), label);
	}
    out:;
    }

  /* Should never return.  */
  panic ("server_loop returned!");
}
