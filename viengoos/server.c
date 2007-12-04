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
  int do_reply = 0;
  l4_thread_id_t to = l4_nilthread;

  for (;;)
    {
      l4_thread_id_t from = l4_anythread;
      l4_msg_tag_t msg_tag;

      /* Only accept untyped items--no strings, no mappings.  */
      l4_accept (L4_UNTYPED_WORDS_ACCEPTOR);
      if (do_reply)
	msg_tag = l4_reply_wait (to, &from);
      else
	msg_tag = l4_wait (&from);

      if (l4_ipc_failed (msg_tag))
	panic ("Receiving message failed: %u", (l4_error_code () >> 1) & 0x7);

      l4_msg_t msg;
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
	      struct exception_info info;
	      info.access = access;
	      info.type = cap_page;

	      l4_msg_t msg;
	      exception_fault_send_marshal (&msg, PTR_TO_ADDR (fault),
					    ip, info);

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
	  l4_pagefault_reply_formulate (&map_item);

	  do_reply = 1;
	  continue;
	}

      struct activity *principal;

  /* Check that the received message contains WORDS untyped words (not
     including the principal!).  */
#define CHECK(words, words64) \
  ({ \
    expected_words = (words) + ((words64) + 1) * ARG64_WORDS; \
    if (l4_untyped_words (msg_tag) != expected_words \
        || l4_typed_words (msg_tag) != 0) \
      { \
        DEBUG (1, "Invalid format for %s: expected %d words, got %d", \
	       rm_method_id_string (label), \
               expected_words, l4_untyped_words (msg_tag)); \
        REPLY (EINVAL); \
      } \
  })

  /* Reply with the error code ERR_ and set the number of untyped
     words *in addition* to the return code to return to WORDS_.  */
#define REPLYW(err_, words_) \
  do \
    { \
      if (! (err_)) \
        /* No error: we should have read all the arguments.  */ \
        assert (args_read == expected_words); \
      l4_msg_clear (msg); \
      l4_msg_put_word (msg, 0, (err_)); \
      l4_msg_set_untyped_words (msg, 1 + (words_)); \
      do_reply = 1; \
      goto out; \
    } \
  while (0)

#define REPLY(err_) REPLYW((err_), 0)

  /* Return the next word.  */
#define ARG() \
  ({ \
    assert (args_read < expected_words); \
    l4_msg_word (msg, args_read ++); \
  })

  /* Return word WORD_.  */
#if L4_WORDSIZE == 32
#define ARG64() \
  ({ \
    union { l4_uint64_t raw; struct { l4_uint32_t word[2]; }; } value_; \
    value_.word[0] = ARG (); \
    value_.word[1] = ARG (); \
    value_.raw; \
  })
#define ARG64_WORDS 2
#else
#define ARG64(word_) ARG(word_)
#define ARG64_WORDS 1
#endif

#define ARG_ADDR() ((addr_t) { ARG64() })

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
	      return ENOENT;
	    }
	  if (! writable)
	    {
	      DEBUG (1, "Capability slot at 0x%llx/%d not writable",
		     addr_prefix (addr), addr_depth (addr));
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

      int args_read = 0;
      /* We set this to WORD64_WORDS; CHECK will set it
	 appropriately.  */
      int expected_words = ARG64_WORDS;

      if (label == RM_putchar)
	{
	  /* We don't expect a principal.  */
	  CHECK (1, -1);

	  int chr = l4_msg_word (msg, 0);
	  putchar (chr);

	  /* No reply needed.  */
	  continue;
	}

      principal = activity;
      addr_t principal_addr = ARG_ADDR ();
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

      error_t err;
      struct folio *folio;
      struct object *object;
      l4_word_t idx;
      l4_word_t type;
      bool r;
      struct cap source;
      addr_t source_addr;
      struct cap *target;
      addr_t target_addr;
      struct cap_addr_trans addr_trans;

      DEBUG (5, "");

      switch (label)
	{
	case RM_folio_alloc:;
	  CHECK (0, 1);
	  struct cap *folio_slot = SLOT (ARG_ADDR ());

	  folio = folio_alloc (principal);
	  if (! folio)
	    REPLY (ENOMEM);

	  r = cap_set (folio_slot, object_to_cap ((struct object *) folio));
	  assert (r);
	  REPLY (0);

	case RM_folio_free:;
	  CHECK (0, 1);

	  folio = (struct folio *) OBJECT (ARG_ADDR (), cap_folio, true);
	  folio_free (principal, folio);

	  REPLY (0);

	case RM_folio_object_alloc:;
	  CHECK (2, 2);

	  addr_t folio_addr = ARG_ADDR ();
	  folio = (struct folio *) OBJECT (folio_addr, cap_folio, true);

	  idx = ARG ();
	  if (idx >= FOLIO_OBJECTS)
	    REPLY (EINVAL);

	  type = ARG ();
	  if (! (CAP_TYPE_MIN <= type && type <= CAP_TYPE_MAX))
	    REPLY (EINVAL);

	  addr_t object_addr = ARG_ADDR ();
	  struct cap *object_slot = NULL;
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
	      r = cap_set (object_slot, object_to_cap (object));
	      assert (r);
	    }

	  REPLY (0);

	case RM_object_slot_copy_out:;
	case RM_object_slot_copy_in:;
	  CHECK (3, 2);

	  addr_t addr = ARG_ADDR ();
	  source = CAP (addr, -1, false);
	  idx = ARG ();

	  if (idx >= cap_type_num_slots[source.type])
	    REPLY (EINVAL);

	  if (source.type == cap_cappage || source.type == cap_rcappage)
	    /* Ensure that idx falls within the subpage.  */
	    {
	      if (idx >= CAP_SUBPAGE_SIZE (&source))
		{
		  DEBUG (1, "index (%d) >= subpage size (%d)",
			 idx, CAP_SUBPAGE_SIZE (&source));
		  REPLY (EINVAL);
		}

	      idx += CAP_SUBPAGE_OFFSET (&source);
	    }

	  object = cap_to_object (principal, &source);
	  if (! object)
	    {
	      DEBUG (1, CAP_FMT " maps to void", CAP_PRINTF (&source));
	      REPLY (EINVAL);
	    }

	  if (label == RM_object_slot_copy_out)
	    {
	      source_addr = addr;

	      source = ((struct cap *) object)[idx];
	      target_addr = ARG_ADDR ();
	      target = SLOT (target_addr);
	    }
	  else
	    {
	      target_addr = addr;

	      source_addr = ARG_ADDR ();
	      source = CAP (source_addr, -1, false);
	      target = &((struct cap *) object)[idx];
	    }

	  goto cap_copy_body;

	case RM_cap_copy:;
	  CHECK (2, 2);

	  target_addr = ARG_ADDR ();
	  target = SLOT (target_addr);

	  source_addr = ARG_ADDR ();
	  source = CAP (source_addr, -1, false);

	cap_copy_body:;

	  l4_word_t flags = ARG ();
	  if ((flags & ~(CAP_COPY_COPY_ADDR_TRANS_SUBPAGE
			 | CAP_COPY_COPY_ADDR_TRANS_GUARD
			 | CAP_COPY_COPY_SOURCE_GUARD)))
	    REPLY (EINVAL);

	  addr_trans.raw = ARG ();

	  DEBUG (4, "(%llx/%d, %llx/%d, %s|%s, {%llx/%d %d/%d})",
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
	  if (r)
	    REPLY (0);
	  else
	    REPLY (EINVAL);

	case RM_object_slot_read:
	  CHECK (1, 1);

	  /* We don't look up the argument directly as we need to
	     respect any subpag specification for cappages.  */
	  source = CAP (ARG_ADDR (), -1, false);
	  l4_word_t idx = ARG ();

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

	  goto cap_read_body;

	case RM_cap_read:;
	  CHECK (0, 1);

	  source = CAP (ARG_ADDR (), -1, false);

	cap_read_body:

	  l4_msg_put_word (msg, 1, source.type);
	  l4_msg_put_word (msg, 2, *(l4_word_t *) &source.addr_trans);

	  REPLYW (0, 2);

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

	    rm_thread_exregs_reply_marshal (&msg, &out);

	    REPLYW (0, sizeof (struct hurd_thread_exregs_out) / 4);
	  }

	case RM_activity_create:
	  {
	    CHECK (3, 3);

	    struct object *child = OBJECT (ARG_ADDR (), cap_activity_control,
					   true);
	    l4_word_t priority = ARG ();
	    l4_word_t weight = ARG ();
	    l4_word_t storage_quota = ARG ();

	    addr_t activity_out_addr = ARG_ADDR ();
	    struct cap *activity_out_slot = NULL;
	    if (! ADDR_IS_VOID (activity_out_addr))
	      activity_out_slot = SLOT (activity_out_addr);

	    addr_t activity_control_out_addr = ARG_ADDR ();
	    struct cap *activity_control_out_slot = NULL;
	    if (! ADDR_IS_VOID (activity_control_out_addr))
	      activity_control_out_slot = SLOT (activity_control_out_addr);

	    err = activity_create (principal, (struct activity *) child,
				   priority, weight, storage_quota);

	    if (activity_out_slot)
	      {
		r = cap_set (activity_out_slot, object_to_cap (child));
		assert (r);
		activity_out_slot->type = cap_activity;
	      }
	    if (activity_control_out_slot)
	      {
		r = cap_set (activity_control_out_slot, object_to_cap (child));
		assert (r);
	      }

	    REPLYW (err, 0);
	  }

	case RM_activity_properties:
	  {
	    CHECK (4, 0);

	    l4_word_t flags = ARG ();
	    l4_word_t priority = ARG ();
	    l4_word_t weight = ARG ();
	    l4_word_t storage_quota = ARG ();

	    if (flags && principal_cap.type != cap_activity_control)
	      REPLY (EPERM);

	    l4_msg_put_word (msg, 1, principal->priority);
	    l4_msg_put_word (msg, 2, principal->weight);
	    l4_msg_put_word (msg, 3, principal->storage_quota);

	    if ((flags & ACTIVITY_PROPERTIES_PRIORITY_SET))
	      principal->priority = priority;
	    if ((flags & ACTIVITY_PROPERTIES_WEIGHT_SET))
	      principal->weight = weight;
	    if ((flags & ACTIVITY_PROPERTIES_STORAGE_QUOTA_SET))
	      principal->storage_quota = storage_quota;

	    REPLYW (0, 3);
	  }

	case RM_exception_collect:
	  {
	    /* We don't expect a principal.  */
	    CHECK (0, 0);

	    debug (4, "Collecting exception: %x.%x",
		   l4_thread_no (from), l4_version (from));

	    /* XXX: Implement me.  */

	    break;
	  }

	default:
	  /* XXX: Don't panic when running production code.  */
	  DEBUG (1, "Didn't handle message from %x.%x with label %d",
		 l4_thread_no (from), l4_version (from), label);
	}

    out:
      if (do_reply)
	l4_msg_load (msg);
    }
}
