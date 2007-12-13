/* server.c - Server loop implementation.
   Copyright (C) 2007 Free Software Foundation, Inc.
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

#include "server.h"

#include "rm.h"

#include "cap.h"
#include "object.h"
#include "thread.h"
#include "activity.h"
#include "viengoos.h"

/* Like debug but also prints the method id.  */
#define DEBUG(level, format, args...)			\
  debug (level, "(%s %d) " format,			\
         l4_is_pagefault (msg_tag) ? "pagefault"	\
         : rm_method_id_string (label), label,		\
	##args)

void
server_loop (void)
{
  error_t err;

  int do_reply = 0;
  l4_thread_id_t to = l4_nilthread;
  l4_msg_t msg;

  for (;;)
    {
      l4_thread_id_t from = l4_anythread;
      l4_msg_tag_t msg_tag;

      /* Only accept untyped items--no strings, no mappings.  */
      l4_accept (L4_UNTYPED_WORDS_ACCEPTOR);
      if (do_reply)
	{
	  l4_msg_load (msg);
	  msg_tag = l4_reply_wait (to, &from);
	}
      else
	msg_tag = l4_wait (&from);

      if (l4_ipc_failed (msg_tag))
	panic ("%s message failed: %u", 
	       l4_error_code () & 1 ? "Receiving" : "Sending",
	       (l4_error_code () >> 1) & 0x7);

      l4_msg_store (msg_tag, msg);
      l4_word_t label;
      label = l4_label (msg_tag);

      /* By default we reply to the sender.  */
      to = from;
      /* Unless explicitly overridden, don't reply.  */
      do_reply = 0;

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

      if (l4_is_pagefault (msg_tag))
	/* The label is not constant: it includes the type of fault.
	   Thus, it is difficult to incorporate it into the case
	   switch below.  */
	{
	  l4_word_t access;
	  l4_word_t ip;
	  l4_word_t fault = l4_pagefault (msg_tag, &access, &ip);
	  bool w = !! (access & L4_FPAGE_WRITABLE);

	  DEBUG (5, "%x.%x page faults at %x (ip = %x)",
		 l4_thread_no (from), l4_version (from), fault, ip);
	  l4_word_t page_addr = fault & ~(PAGESIZE - 1);

	  bool writable;
	  struct cap cap;
	  struct object *page = NULL;

	  cap = object_lookup_rel (activity, &thread->aspace,
				   ADDR (page_addr, ADDR_BITS - PAGESIZE_LOG2),
				   w ? cap_page : cap_rpage,
				   &writable);
	  if (cap.type != cap_void)
	    page = cap_to_object (activity, &cap);

	  bool raise_fault = false;
	  if (! page)
	    {
	      raise_fault = true;
	      DEBUG (5, "%x.%x raised fault (ip: %x; fault: %x.%c)!",
		     l4_thread_no (from), l4_version (from), ip,
		     fault, w ? 'w' : 'r');
	    }
	  else if (w && ! writable)
	    /* Only allow permitted writes through.  */
	    {
	      raise_fault = true;
	      DEBUG (5, "%x.%x raised access fault (ip: %x; fault: %x.%c)!",
		     l4_thread_no (from), l4_version (from), ip,
		     fault, w ? 'w' : 'r');
	    }

	  if (raise_fault)
	    {
	      l4_word_t c = _L4_XCHG_REGS_DELIVER;
	      l4_thread_id_t targ = thread->tid;
	      l4_word_t sp = 0;
	      l4_word_t dummy = 0;
	      _L4_exchange_registers (&targ, &c,
				      &sp, &dummy, &dummy, &dummy, &dummy);

	      struct exception_info info;
	      info.access = access;
	      info.type = cap_page;

	      l4_msg_t msg;
	      exception_fault_send_marshal (&msg, PTR_TO_ADDR (fault),
					    sp, ip, info);

	      thread_raise_exception (activity, thread, &msg);

	      continue;
	    }

	  // DEBUG ("Replying with addr %x", (uintptr_t) page);
	  l4_map_item_t map_item
	    = l4_map_item (l4_fpage_add_rights (l4_fpage ((uintptr_t) page,
							  PAGESIZE),
						access),
			   page_addr);

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

  /* Return the capability slot corresponding to address ADDR.  */
      error_t SLOT_ (addr_t addr, struct cap **capp)
	{
	  bool writable;
	  *capp = slot_lookup_rel (activity, &thread->aspace,
				   addr, -1, &writable);
	  if (! *capp)
	    {
	      DEBUG (1, "No capability slot at 0x%llx/%d",
		     addr_prefix (addr), addr_depth (addr));
	      as_dump_from (activity, &thread->aspace, "");
	      return ENOENT;
	    }
	  if (! writable)
	    {
	      DEBUG (1, "Capability slot at 0x%llx/%d not writable",
		     addr_prefix (addr), addr_depth (addr));
	      as_dump_from (activity, &thread->aspace, "");
	      return EPERM;
	    }

	  return 0;
      }
#define SLOT(addr_) \
  ({ struct cap *SLOT_ret; \
     error_t err = SLOT_ (addr_, &SLOT_ret); \
     if (err) \
       REPLY (err); \
     SLOT_ret; \
  })
      /* Return a cap referencing the object at address ADDR of the
	 callers capability space if it is of type TYPE (-1 = don't care).
	 Whether the object is writable is stored in *WRITABLEP_.  */
      error_t CAP_ (addr_t addr, int type, bool require_writable,
		    struct cap *cap)
	{
	  bool writable = true;
	  *cap = cap_lookup_rel (principal, &thread->aspace, addr,
				 type, require_writable ? &writable : NULL);
	  if (type != -1 && ! cap_types_compatible (cap->type, type))
	    {
	      DEBUG (1, "Addr 0x%llx/%d does not reference object of type %s",
		     addr_prefix (addr), addr_depth (addr),
		     cap_type_string (type));
	      as_dump_from (activity, &thread->aspace, "");
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
#define CAP(addr_, type_, require_writable_) \
  ({ struct cap CAP_ret; \
     error_t err = CAP_ (addr_, type_, require_writable_, &CAP_ret); \
     if (err) \
       REPLY (err); \
     CAP_ret; \
  })

      error_t OBJECT_ (addr_t addr, int type, bool require_writable,
		       struct object **objectp)
	{
	  struct cap cap;
	  error_t err = CAP_ (addr, type, require_writable, &cap);
	  if (err)
	    return err;

	  *objectp = cap_to_object (principal, &cap);
	  if (! *objectp)
	    {
	      DEBUG (4, "Addr 0x%llx/%d, dangling pointer",
		     addr_prefix (addr), addr_depth (addr));
	      return ENOENT;
	    }

	  return 0;
	}
#define OBJECT(addr_, type_, require_writable_) \
  ({ struct object *OBJECT_ret; \
     error_t err = OBJECT_ (addr_, type_, require_writable_, &OBJECT_ret); \
     if (err) \
       REPLY (err); \
     OBJECT_ret; \
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
	  principal_cap = CAP (principal_addr, cap_activity, false);
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

      addr_t folio_addr;
      struct folio *folio;
      addr_t object_addr;
      struct cap object_cap;
      struct cap *object_slot;
      struct object *object;
      l4_word_t idx;
      l4_word_t type;
      bool r;
      addr_t source_addr;
      struct cap source;
      addr_t target_addr;
      struct cap *target;
      l4_word_t flags;
      struct cap_addr_trans addr_trans;

      DEBUG (5, "");

      switch (label)
	{
	case RM_folio_alloc:
	  err = rm_folio_alloc_send_unmarshal (&msg, &principal_addr,
					       &folio_addr);
	  if (err)
	    REPLY (err);

	  struct cap *folio_slot = SLOT (folio_addr);

	  folio = folio_alloc (principal);
	  if (! folio)
	    REPLY (ENOMEM);

	  r = cap_set (principal,
		       folio_slot, object_to_cap ((struct object *) folio));
	  assert (r);

	  rm_folio_alloc_reply_marshal (&msg);
	  break;

	case RM_folio_free:
	  err = rm_folio_free_send_unmarshal (&msg, &principal_addr,
					      &folio_addr);
	  if (err)
	    REPLY (err);

	  folio = (struct folio *) OBJECT (folio_addr, cap_folio, true);
	  folio_free (principal, folio);

	  rm_folio_free_reply_marshal (&msg);
	  break;

	case RM_folio_object_alloc:
	  err = rm_folio_object_alloc_send_unmarshal (&msg, &principal_addr,
						      &folio_addr, &idx,
						      &type, &object_addr);
	  if (err)
	    REPLY (err);

	  folio = (struct folio *) OBJECT (folio_addr, cap_folio, true);

	  if (idx >= FOLIO_OBJECTS)
	    REPLY (EINVAL);

	  if (! (CAP_TYPE_MIN <= type && type <= CAP_TYPE_MAX))
	    REPLY (EINVAL);

	  object_slot = NULL;
	  if (! ADDR_IS_VOID (object_addr))
	    object_slot = SLOT (object_addr);

	  DEBUG (4, "(folio: %llx/%d, idx: %d, type: %s, target: %llx/%d)",
		 addr_prefix (folio_addr), addr_depth (folio_addr),
		 idx, cap_type_string (type),
		 addr_prefix (object_addr), addr_depth (object_addr));

	  folio_object_alloc (principal, folio, idx, type,
			      type == cap_void ? NULL : &object);

	  if (type != cap_void && object_slot)
	    {
	      r = cap_set (principal, object_slot, object_to_cap (object));
	      assert (r);
	    }

	  rm_folio_object_alloc_reply_marshal (&msg);
	  break;

	case RM_object_slot_copy_out:
	  err = rm_object_slot_copy_out_send_unmarshal
	    (&msg, &principal_addr,
	     &source_addr, &idx, &target_addr, &flags, &addr_trans);
	  if (err)
	    REPLY (err);

	  object_cap = CAP (source_addr, -1, false);

	  /* Fall through.  */

	case RM_object_slot_copy_in:
	  if (label == RM_object_slot_copy_in)
	    {
	      err = rm_object_slot_copy_in_send_unmarshal
		(&msg, &principal_addr,
		 &target_addr, &idx, &source_addr, &flags, &addr_trans);
	      if (err)
		REPLY (err);

	      object_cap = CAP (target_addr, -1, true);
	    }

	  if (idx >= cap_type_num_slots[object_cap.type])
	    REPLY (EINVAL);

	  if (object_cap.type == cap_cappage || object_cap.type == cap_rcappage)
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
	      target = SLOT (target_addr);
	    }
	  else
	    {
	      source = CAP (source_addr, -1, false);
	      target = &((struct cap *) object)[idx];
	    }

	  goto cap_copy_body;

	case RM_cap_copy:
	  err = rm_cap_copy_send_unmarshal (&msg, &principal_addr,
					    &target_addr, &source_addr,
					    &flags, &addr_trans);
	  if (err)
	    REPLY (err);

	  target = SLOT (target_addr);
	  source = CAP (source_addr, -1, false);

	cap_copy_body:;

	  if ((flags & ~(CAP_COPY_COPY_ADDR_TRANS_SUBPAGE
			 | CAP_COPY_COPY_ADDR_TRANS_GUARD
			 | CAP_COPY_COPY_SOURCE_GUARD)))
	    REPLY (EINVAL);

	  DEBUG (4, "(target: %llx/%d, source: %llx/%d, "
		 "%s|%s, {%llx/%d %d/%d})",
		 addr_prefix (target_addr), addr_depth (target_addr),
		 addr_prefix (source_addr), addr_depth (source_addr),
		 flags & CAP_COPY_COPY_ADDR_TRANS_GUARD ? "copy trans"
		 : (flags & CAP_COPY_COPY_SOURCE_GUARD ? "source"
		    : "preserve"),
		 flags & CAP_COPY_COPY_ADDR_TRANS_SUBPAGE ? "copy"
		 : "preserve",
		 CAP_ADDR_TRANS_GUARD (addr_trans),
		 CAP_ADDR_TRANS_GUARD_BITS (addr_trans),
		 CAP_ADDR_TRANS_SUBPAGE (addr_trans),
		 CAP_ADDR_TRANS_SUBPAGES (addr_trans));

	  bool r = cap_copy_x (principal,
			       target, ADDR_VOID,
			       source, ADDR_VOID,
			       flags, addr_trans);
	  if (! r)
	    REPLY (EINVAL);

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

	case RM_object_slot_read:
	  err = rm_object_slot_read_send_unmarshal (&msg,
						    &principal_addr,
						    &source_addr, &idx);
	  if (err)
	    REPLY (err);

	  /* We don't look up the argument directly as we need to
	     respect any subpag specification for cappages.  */
	  source = CAP (source_addr, -1, false);

	  object = cap_to_object (activity, &source);
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
					     source.addr_trans);
	  break;


	case RM_cap_read:;

	  err = rm_cap_read_send_unmarshal (&msg, &principal_addr,
					    &source_addr);

	  source = CAP (source_addr, -1, false);

	  rm_cap_read_reply_marshal (&msg, source.type, source.addr_trans);
	  break;

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

	    struct thread *t
	      = (struct thread *) OBJECT (target, cap_thread, true);

	    struct cap *aspace = NULL;
	    struct cap aspace_cap;
	    if ((HURD_EXREGS_SET_ASPACE & control))
	      {
		aspace_cap = CAP (in.aspace, -1, false);
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
		    a_cap = CAP (in.activity, cap_activity, false);
		    a = &a_cap;
		  }
	      }

	    struct cap *exception_page = NULL;
	    struct cap exception_page_cap;
	    if ((HURD_EXREGS_SET_EXCEPTION_PAGE & control))
	      {
		exception_page_cap = CAP (in.exception_page, cap_page, true);
		exception_page = &exception_page_cap;
	      }

	    struct cap *aspace_out = NULL;
	    if ((HURD_EXREGS_GET_REGS & control)
		&& ! ADDR_IS_VOID (in.aspace_out))
	      aspace_out = SLOT (in.aspace_out);

	    struct cap *activity_out = NULL;
	    if ((HURD_EXREGS_GET_REGS & control)
		&& ! ADDR_IS_VOID (in.activity_out))
	      activity_out = SLOT (in.activity_out);

	    struct cap *exception_page_out = NULL;
	    if ((HURD_EXREGS_GET_REGS & control)
		&& ! ADDR_IS_VOID (in.exception_page_out))
	      exception_page_out = SLOT (in.exception_page_out);

	    struct hurd_thread_exregs_out out;
	    out.sp = in.sp;
	    out.ip = in.ip;
	    out.eflags = in.eflags;
	    out.user_handle = in.user_handle;

	    err = thread_exregs (principal, t, control,
				 aspace, in.aspace_addr_trans_flags,
				 in.aspace_addr_trans, a, exception_page,
				 &out.sp, &out.ip,
				 &out.eflags, &out.user_handle,
				 aspace_out, activity_out,
				 exception_page_out);
	    if (err)
	      REPLY (err);

	    rm_thread_exregs_reply_marshal (&msg, out);

	    break;
	  }

	case RM_activity_properties:
	  {
	    l4_word_t flags;
	    l4_word_t priority;
	    l4_word_t weight;
	    l4_word_t storage_quota;

	    err = rm_activity_properties_send_unmarshal (&msg,
							 &principal_addr,
							 &flags,
							 &priority, &weight,
							 &storage_quota);
	    if (err)
	      REPLY (err);

	    if (flags && principal_cap.type != cap_activity_control)
	      REPLY (EPERM);

	    rm_activity_properties_reply_marshal (&msg,
						  principal->priority,
						  principal->weight,
						  principal->storage_quota);

	    if ((flags & ACTIVITY_PROPERTIES_PRIORITY_SET))
	      principal->priority = priority;
	    if ((flags & ACTIVITY_PROPERTIES_WEIGHT_SET))
	      principal->weight = weight;
	    if ((flags & ACTIVITY_PROPERTIES_STORAGE_QUOTA_SET))
	      principal->storage_quota = storage_quota;

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
	    addr_t thread_addr;
	    err = rm_as_dump_send_unmarshal (&msg, &principal_addr,
					     &thread_addr);
	    if (err)
	      REPLY (err);

	    struct thread *t;

	    if (ADDR_IS_VOID (thread_addr))
	      t = thread;
	    else
	      t = (struct thread *) OBJECT (thread_addr, cap_thread, true);

	    as_dump_from (principal, &t->aspace, "");

	    rm_as_dump_reply_marshal (&msg);

	    break;
	  }

	default:
	  /* XXX: Don't panic when running production code.  */
	  DEBUG (1, "Didn't handle message from %x.%x with label %d",
		 l4_thread_no (from), l4_version (from), label);
	}
    out:;
    }
}
