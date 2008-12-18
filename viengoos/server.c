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
#include <viengoos/cap.h>
#include <hurd/stddef.h>
#include <viengoos/thread.h>
#include <viengoos/activity.h>
#include <viengoos/futex.h>
#include <hurd/trace.h>
#include <hurd/as.h>
#include <viengoos/ipc.h>

#include "server.h"

#include <viengoos/misc.h>

#include "output.h"
#include "cap.h"
#include "object.h"
#include "thread.h"
#include "activity.h"
#include "messenger.h"
#include "viengoos.h"
#include "profile.h"

#ifndef NDEBUG
struct futex_waiter_list futex_waiters;
#endif

#ifndef NDEBUG

struct trace_buffer rpc_trace = TRACE_BUFFER_INIT ("rpcs", 0,
						   true, false, false);

/* Like debug but also prints the method id and saves to the trace
   buffer if level is less than or equal to 4.  */
# define DEBUG(level, format, args...)					\
  do									\
    {									\
      if (level <= 4)							\
	trace_buffer_add (&rpc_trace, "(%x %s %d) " format,		\
			  thread->tid,					\
			  l4_is_pagefault (msg_tag) ? "pagefault"	\
			  : label == 8194 ? "IPC"			\
			  : vg_method_id_string (label),		\
			  label,					\
			  ##args);					\
      debug (level, "(%x %s:%d %d) " format,				\
	     thread->tid, l4_is_pagefault (msg_tag) ? "pagefault"	\
	     : label == 8194 ? "IPC" : vg_method_id_string (label),	\
	     __LINE__, label,						\
	     ##args);							\
    }									\
  while (0)

#else
# define DEBUG(level, format, args...)					\
      debug (level, "(%x %s:%d %d) " format,				\
	     thread->tid, l4_is_pagefault (msg_tag) ? "pagefault"	\
	     : label == 8194 ? "IPC" : vg_method_id_string (label),	\
	     __LINE__, label,						\
	     ##args)
#endif

#define PAGEFAULT_METHOD 2

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

  /* Profiling.  */
  int method = -1;

  for (;;)
    {
      if (method != -1)
	{
	  profile_end (method);
	  method = -1;
	}

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
	      debug (0, "No IPCs for some time.  Deadlock?");

	      struct messenger *messenger;
	      while ((messenger = futex_waiter_list_head (&futex_waiters)))
		{
		  object_wait_queue_unlink (root_activity, messenger);
		  rpc_error_reply (root_activity, messenger, EDEADLK);
		}

	      trace_buffer_dump (&rpc_trace, 0);
	      rpc_trace_just_dumped = true;
	    }
#endif

	  debug (4, "%s %x failed: %u", 
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
      uintptr_t label;
      label = l4_label (msg_tag);

      /* By default we reply to the sender.  */
      to = from;
      /* Unless explicitly overridden, don't reply.  */
      do_reply = 0;

      debug (5, "%x (p: %d, %x) sent %s (%x)",
	     from, l4_ipc_propagated (msg_tag), l4_actual_sender (),
	     (l4_is_pagefault (msg_tag) ? "fault handler"
	      : vg_method_id_string (label)),
	     label);

      if (l4_version (l4_myself ()) == l4_version (from))
	/* Message from a kernel thread.  */
	panic ("Kernel thread %x (propagated: %d, actual: %x) sent %s? (%x)!",
	       from, l4_ipc_propagated (msg_tag), l4_actual_sender (),
	       (l4_is_pagefault (msg_tag) ? "fault handler"
		      : vg_method_id_string (label)),
	       label);

      ss_mutex_lock (&kernel_lock);
      have_lock = true;


      /* Start timer.  */
      if (l4_is_pagefault (msg_tag))
	method = PAGEFAULT_METHOD;
      else
	method = label;
      profile_start (method,
		     method == PAGEFAULT_METHOD ? "fault handler"
		     : vg_method_id_string (method), NULL);

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
	= (struct activity *) vg_cap_to_object (root_activity,
					     &thread->activity);
      if (! activity)
	{
	  DEBUG (1, "Caller has no assigned activity");
	  continue;
	}
      if (object_type ((struct vg_object *) activity)
	  != vg_cap_activity_control)
	{
	  DEBUG (1, "Caller's activity slot contains a %s,"
		 "not an activity_control",
		 vg_cap_type_string
		  (object_type ((struct vg_object *) activity)));
	  continue;
	}

      if (l4_is_pagefault (msg_tag))
	/* The label is not constant: it includes the type of fault.
	   Thus, it is difficult to incorporate it into the case
	   switch below.  */
	{
	  uintptr_t access;
	  uintptr_t ip;
	  uintptr_t fault = l4_pagefault (msg_tag, &access, &ip);
	  bool write_fault = !! (access & L4_FPAGE_WRITABLE);

	  DEBUG (4, "%s fault at %x (ip: %x)",
		 write_fault ? "Write" : "Read", fault, ip);

	  uintptr_t page_addr = fault & ~(PAGESIZE - 1);

	  struct vg_cap cap;
	  bool writable;
	  cap = as_object_lookup_rel (activity, &thread->aspace,
				      vg_addr_chop (VG_PTR_TO_ADDR (page_addr),
						 PAGESIZE_LOG2),
				      write_fault ? vg_cap_page : vg_cap_rpage,
				      &writable);

	  assert (cap.type == vg_cap_void
		  || cap.type == vg_cap_page
		  || cap.type == vg_cap_rpage);

	  bool discarded = false;
	  if (write_fault && ! writable)
	    {
	      debug (5, "Access fault at %x", page_addr);
	      goto do_fault;
	    }

	  if (! writable && cap.discardable)
	    {
	      DEBUG (4, "Ignoring discardable predicate for cap designating "
		     VG_OID_FMT " (%s)",
		     VG_OID_PRINTF (cap.oid), vg_cap_type_string (cap.type));
	      cap.discardable = false;
	    }

	  struct vg_object *page = vg_cap_to_object (activity, &cap);
	  if (! page && cap.type != vg_cap_void)
	    /* It's not in-memory.  See if it was discarded.  If not,
	       load it using vg_cap_to_object.  */
	    {
	      int object = (cap.oid % (VG_FOLIO_OBJECTS + 1)) - 1;
	      vg_oid_t foid = cap.oid - object - 1;
	      struct vg_folio *folio
		= (struct vg_folio *) object_find (activity, foid,
						VG_OBJECT_POLICY_DEFAULT);
	      assert (folio);
	      assert (object_type ((struct vg_object *) folio) == vg_cap_folio);

	      if (cap.version == folio_object_version (folio, object))
		{
		  if (folio_object_discarded (folio, object))
		    {
		      DEBUG (4, VG_OID_FMT " (%s) was discarded",
			     VG_OID_PRINTF (cap.oid),
			     vg_cap_type_string (vg_folio_object_type (folio,
								       object)));

		      assert (! folio_object_content (folio, object));

		      discarded = true;

		      DEBUG (5, "Raising discarded fault at %x", page_addr);
		    }
		}
	    }

	  if (! page)
	    {
	    do_fault:
	      DEBUG (4, "Reflecting fault (ip: %x; fault: %x.%c%s)!",
		     ip, fault, write_fault ? 'w' : 'r',
		     discarded ? " discarded" : "");

	      uintptr_t c = _L4_XCHG_REGS_DELIVER;
	      l4_thread_id_t targ = thread->tid;
	      uintptr_t sp = 0;
	      uintptr_t dummy = 0;
	      _L4_exchange_registers (&targ, &c,
				      &sp, &dummy, &dummy, &dummy, &dummy);

	      struct vg_activation_fault_info info;
	      info.access = access;
	      info.type = write_fault ? vg_cap_page : vg_cap_rpage;

	      info.discarded = discarded;

	      vg_activation_fault_send_marshal (reply_buffer,
						VG_PTR_TO_ADDR (fault),
						sp, ip, info, VG_ADDR_VOID);
	      thread_raise_exception (activity, thread, reply_buffer);

	      continue;
	    }

	  DEBUG (4, "%s fault at " DEBUG_BOLD ("%x") " (ip=%x), "
		 "replying with %p(r%s)",
		 write_fault ? "Write" : "Read", fault, ip, page,
		 writable ? "w" : "");

	  object_to_object_desc (page)->mapped = true;

	  access = L4_FPAGE_READABLE;
	  if (writable)
	    access |= L4_FPAGE_WRITABLE;

	  l4_map_item_t map_item
	    = l4_map_item
	       (l4_fpage_add_rights (l4_fpage ((uintptr_t) page, PAGESIZE),
				     access),
		page_addr);

	  /* Formulate the reply message.  */
	  l4_pagefault_reply_formulate_in (msg, &map_item);

#if 0
	  int count = 0;
	  while (sizeof (l4_map_item_t) / sizeof (l4_word_t)
		 < (L4_NUM_MRS - 1
		    - l4_untyped_words (l4_msg_msg_tag (msg))
		    - l4_typed_words (l4_msg_msg_tag (msg))))
	    {
	      page_addr += PAGESIZE;

	      cap = as_object_lookup_rel (activity, &thread->aspace,
					  vg_addr_chop (VG_PTR_TO_ADDR (page_addr),
						     PAGESIZE_LOG2),
					  vg_cap_rpage, &writable);

	      if (cap.type != vg_cap_page && cap.type != vg_cap_rpage)
		break;

	      if (! writable && cap.discardable)
		cap.discardable = false;

	      struct vg_object *page = vg_cap_to_object (activity, &cap);
	      if (! page)
		break;

	      object_to_object_desc (page)->mapped = true;

	      access = L4_FPAGE_READABLE;
	      if (writable)
		access |= L4_FPAGE_WRITABLE;

	      l4_fpage_t fpage = l4_fpage ((uintptr_t) page, PAGESIZE);
	      fpage = l4_fpage_add_rights (fpage, access);
	      l4_map_item_t map_item = l4_map_item (fpage, page_addr);

	      l4_msg_append_map_item (msg, map_item);

	      DEBUG (5, "Prefaulting " DEBUG_BOLD ("%x") " <- %p (%x/%x/%x) %s",
		     page_addr,
		     page, l4_address (fpage), l4_size (fpage),
		     l4_rights (fpage), vg_cap_type_string (cap.type));

	      count ++;
	    }

	  if (count > 0)
	    DEBUG (5, "Prefaulted %d pages (%d/%d)", count,
		   l4_untyped_words (l4_msg_msg_tag (msg)),
		   l4_typed_words (l4_msg_msg_tag (msg)));
#endif

	  do_reply = 1;
	  continue;
	}

      struct activity *principal;

  /* Create a message indicating an error with the error code ERR_.
     Go to the start of the server loop.  */
#define REPLY(err_)						\
      do							\
	{							\
	  if (err_)						\
	    DEBUG (1, DEBUG_BOLD ("Returning error %d to %x"),	\
		   err_, from);					\
	  l4_msg_clear (msg);					\
	  l4_msg_put_word (msg, 0, (err_));			\
	  l4_msg_set_untyped_words (msg, 1);			\
	  do_reply = 1;						\
	  goto out;						\
	}							\
      while (0)

      /* Return the capability slot corresponding to address ADDR in
	 the address space rooted at ROOT.  */
      error_t SLOT_ (struct vg_cap *root, vg_addr_t addr, struct vg_cap **capp)
	{
	  bool w;
	  if (! as_slot_lookup_rel_use (activity, root, addr,
					({
					  w = writable;
					  *capp = slot;
					})))
	    {
	      DEBUG (0, "No capability slot at 0x%llx/%d",
		     vg_addr_prefix (addr), vg_addr_depth (addr));
	      as_dump_from (activity, root, "");
	      return ENOENT;
	    }
	  if (! w)
	    {
	      DEBUG (1, "Capability slot at 0x%llx/%d not writable",
		     vg_addr_prefix (addr), vg_addr_depth (addr));
	      as_dump_from (activity, root, "");
	      return EPERM;
	    }

	  return 0;
      }
#define SLOT(root_, addr_)				\
      ({						\
	struct vg_cap *SLOT_ret;				\
	error_t err = SLOT_ (root_, addr_, &SLOT_ret);	\
	if (err)					\
	  REPLY (err);					\
	SLOT_ret;					\
      })

      /* Return a cap referencing the object at address ADDR of the
	 callers capability space if it is of type TYPE (-1 = don't care).
	 Whether the object is writable is stored in *WRITABLEP_.  */
      error_t CAP_ (struct vg_cap *root,
		    vg_addr_t addr, int type, bool require_writable,
		    struct vg_cap *cap)
	{
	  bool writable = true;
	  *cap = as_cap_lookup_rel (principal, root, addr,
				    type, require_writable ? &writable : NULL);
	  if (type != -1 && ! vg_cap_types_compatible (cap->type, type))
	    {
	      DEBUG (1, "Addr 0x%llx/%d does not reference object of "
		     "type %s but %s",
		     vg_addr_prefix (addr), vg_addr_depth (addr),
		     vg_cap_type_string (type), vg_cap_type_string (cap->type));
	      as_dump_from (activity, root, "");
	      return ENOENT;
	    }

	  if (require_writable && ! writable)
	    {
	      DEBUG (1, "Addr " VG_ADDR_FMT " not writable",
		     VG_ADDR_PRINTF (addr));
	      return EPERM;
	    }

	  return 0;
	}
#define CAP(root_, addr_, type_, require_writable_)			\
      ({								\
	struct vg_cap CAP_ret;						\
	error_t err = CAP_ (root_, addr_, type_, require_writable_,	\
			    &CAP_ret);					\
	if (err)							\
	  REPLY (err);							\
	CAP_ret;							\
      })

      error_t OBJECT_ (struct vg_cap *root,
		       vg_addr_t addr, int type, bool require_writable,
		       struct vg_object **objectp, bool *writable)
	{
	  bool w = true;
	  struct vg_cap cap;
	  cap = as_object_lookup_rel (principal, root, addr, type, &w);
	  if (type != -1 && ! vg_cap_types_compatible (cap.type, type))
	    {
	      DEBUG (0, "Addr 0x%llx/%d does not reference object of "
		     "type %s but %s",
		     vg_addr_prefix (addr), vg_addr_depth (addr),
		     vg_cap_type_string (type), vg_cap_type_string (cap.type));
	      return ENOENT;
	    }

	  if (writable)
	    *writable = w;

	  if (require_writable && ! w)
	    {
	      DEBUG (0, "Addr " VG_ADDR_FMT " not writable",
		     VG_ADDR_PRINTF (addr));
	      return EPERM;
	    }

	  *objectp = vg_cap_to_object (principal, &cap);
	  if (! *objectp)
	    {
	      do_debug (4)
		DEBUG (0, "Addr " VG_ADDR_FMT " contains a dangling pointer: "
		       VG_CAP_FMT,
		       VG_ADDR_PRINTF (addr), VG_CAP_PRINTF (&cap));
	      return ENOENT;
	    }

	  return 0;
	}
#define OBJECT(root_, addr_, type_, require_writable_, writablep_)	\
      ({								\
	struct vg_object *OBJECT_ret;					\
	error_t err = OBJECT_ (root_, addr_, type_, require_writable_,	\
			       &OBJECT_ret, writablep_);		\
	if (err)							\
	  REPLY (err);							\
	OBJECT_ret;							\
      })

      /* Find an address space root.  If VG_ADDR_VOID, the current
	 thread's.  Otherwise, the object identified by ROOT_ADDR_ in
	 the caller's address space.  If that is a thread object, then
	 it's address space root.  */
#define ROOT(root_addr_)						\
      ({								\
	struct vg_cap *root_;						\
	if (VG_ADDR_IS_VOID (root_addr_))					\
	  root_ = &thread->aspace;					\
	else								\
	  {								\
	    /* This is annoying: 1) a thread could be in a folio so we  \
	       can't directly lookup the slot, 2) we only want the      \
	       thread if it matches the guard exactly.  */		\
	    struct vg_object *t_;						\
	    error_t err = OBJECT_ (&thread->aspace, root_addr_,		\
				   vg_cap_thread, true, &t_, NULL);	\
	    if (! err)							\
	      root_ = &((struct thread *) t_)->aspace;			\
	    else							\
	      root_ = SLOT (&thread->aspace, root_addr_);		\
	  }								\
	DEBUG (4, "root: " VG_CAP_FMT, VG_CAP_PRINTF (root_));		\
									\
	root_;								\
      })

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

#define ARG_ADDR(word_) ((vg_addr_t) { ARG64(word_) })

      if (label == 2132)
	/* write.  */
	{
	  int len = msg[1];
	  char *buffer = (char *) &msg[2];
	  buffer[len] = 0;
	  s_printf ("%s", buffer);
	  continue;
	}

      if (label != 8194)
	{
	  DEBUG (0, "Invalid label: %d", label);
	  continue;
	}

      int i = 0;
      uintptr_t flags = ARG (i);
      i ++;
      vg_addr_t recv_activity = ARG_ADDR (i);
      i += ARG64_WORDS;
      vg_addr_t recv_messenger = ARG_ADDR (i);
      i += ARG64_WORDS;
      vg_addr_t recv_buf = ARG_ADDR (i);
      i += ARG64_WORDS;
      vg_addr_t recv_inline_cap = ARG_ADDR (i);
      i += ARG64_WORDS;

      vg_addr_t send_activity = ARG_ADDR (i);
      i += ARG64_WORDS;
      vg_addr_t target_messenger = ARG_ADDR (i);
      i += ARG64_WORDS;

      vg_addr_t send_messenger = ARG_ADDR (i);
      i += ARG64_WORDS;
      vg_addr_t send_buf = ARG_ADDR (i);
      i += ARG64_WORDS;

      uintptr_t inline_word1 = ARG (i);
      i ++;
      uintptr_t inline_word2 = ARG (i);
      i ++;
      vg_addr_t inline_cap = ARG_ADDR (i);

#ifndef NDEBUG
      /* Get the label early to improve debugging output in case the
	 target is invalid.  */
      if ((flags & VG_IPC_SEND))
	{
	  if ((flags & VG_IPC_SEND_INLINE))
	    label = inline_word1;
	  else
	    {
	      principal = activity;

	      struct vg_cap cap = VG_CAP_VOID;
	      if (! VG_ADDR_IS_VOID (send_buf))
		/* Caller provided a send buffer.  */
		CAP_ (&thread->aspace, send_buf, vg_cap_page, true, &cap);
	      else
		{
		  struct vg_object *object = NULL;
		  OBJECT_ (&thread->aspace, send_messenger,
			   vg_cap_messenger, true, &object, NULL);
		  if (object)
		    cap = ((struct messenger *) object)->buffer;
		}
		
	      struct vg_message *message;
	      message = (struct vg_message *) vg_cap_to_object (principal,
							     &cap);
	      if (message)
		label = vg_message_word (message, 0);
	    }
	}
#endif

      DEBUG (4, "flags: %s%s%s%s%s%s %s%s%s%s%s%s %s %s%s%s%s(%x),"
	     "recv (" VG_ADDR_FMT ", " VG_ADDR_FMT ", " VG_ADDR_FMT "), "
	     "send (" VG_ADDR_FMT ", " VG_ADDR_FMT ", " VG_ADDR_FMT ", " VG_ADDR_FMT "), "
	     "inline (" VG_ADDR_FMT "; %x, %x, " VG_ADDR_FMT ")",
	     (flags & VG_IPC_RECEIVE) ? "R" : "-",
	     (flags & VG_IPC_RECEIVE_NONBLOCKING) ? "N" : "B",
	     (flags & VG_IPC_RECEIVE_ACTIVATE) ? "A" : "-",
	     (flags & VG_IPC_RECEIVE_SET_THREAD_TO_CALLER) ? "T" : "-",
	     (flags & VG_IPC_RECEIVE_SET_ASROOT_TO_CALLERS) ? "A" : "-",
	     (flags & VG_IPC_RECEIVE_INLINE) ? "I" : "-",
	     (flags & VG_IPC_SEND) ? "S" : "-",
	     (flags & VG_IPC_SEND_NONBLOCKING) ? "N" : "B",
	     (flags & VG_IPC_SEND_ACTIVATE) ? "A" : "-",
	     (flags & VG_IPC_SEND_SET_THREAD_TO_CALLER) ? "T" : "-",
	     (flags & VG_IPC_SEND_SET_ASROOT_TO_CALLERS) ? "A" : "-",
	     (flags & VG_IPC_SEND_INLINE) ? "I" : "-",
	     (flags & VG_IPC_RETURN) ? "R" : "-",
	     (flags & VG_IPC_RECEIVE_INLINE_CAP1) ? "C" : "-",
	     (flags & VG_IPC_SEND_INLINE_WORD1) ? "1" : "-",
	     (flags & VG_IPC_SEND_INLINE_WORD2) ? "2" : "-",
	     (flags & VG_IPC_SEND_INLINE_CAP1) ? "C" : "-",
	     flags,
	     VG_ADDR_PRINTF (recv_activity), VG_ADDR_PRINTF (recv_messenger),
	     VG_ADDR_PRINTF (recv_buf),
	     VG_ADDR_PRINTF (send_activity), VG_ADDR_PRINTF (target_messenger),
	     VG_ADDR_PRINTF (send_messenger), VG_ADDR_PRINTF (send_buf),
	     VG_ADDR_PRINTF (recv_inline_cap),
	     inline_word1, inline_word2, VG_ADDR_PRINTF (inline_cap));

      if ((flags & VG_IPC_RECEIVE))
	/* IPC includes a receive phase.  */
	{
	  principal = activity;
	  if (! VG_ADDR_IS_VOID (recv_activity))
	    {
	      principal = (struct activity *) OBJECT (&thread->aspace,
						      recv_activity,
						      vg_cap_activity, false,
						      NULL);
	      if (! principal)
		{
		  DEBUG (0, "Invalid receive activity.");
		  REPLY (ENOENT);
		}
	    }

	  struct messenger *messenger
	    = (struct messenger *) OBJECT (&thread->aspace,
					   recv_messenger, vg_cap_messenger,
					   true, NULL);
	  if (! messenger)
	    {
	      DEBUG (0, "IPC includes receive phase, however, "
		     "no receive messenger provided.");
	      REPLY (EINVAL);
	    }

	  if ((flags & VG_IPC_RECEIVE_INLINE))
	    {
	      messenger->out_of_band = false;
	      if ((flags & VG_IPC_RECEIVE_INLINE_CAP1))
		messenger->inline_caps[0] = recv_inline_cap;
	    }
	  else
	    {
	      messenger->out_of_band = true;
	      if (unlikely (! VG_ADDR_IS_VOID (recv_buf)))
		/* Associate RECV_BUF with RECV_MESSENGER.  */
		messenger->buffer = CAP (&thread->aspace, recv_buf,
					 vg_cap_page, true);
	    }

	  if (unlikely ((flags & VG_IPC_RECEIVE_SET_THREAD_TO_CALLER)))
	    messenger->thread = object_to_cap ((struct vg_object *) thread);

	  if (unlikely ((flags & VG_IPC_RECEIVE_SET_ASROOT_TO_CALLERS)))
	    messenger->as_root = thread->aspace;

	  messenger->activate_on_receive = (flags & VG_IPC_RECEIVE_ACTIVATE);

	  /* See if there is a messenger trying to send to
	     MESSENGER.  */
	  struct messenger *sender;
	  object_wait_queue_for_each (principal,
				      (struct vg_object *) messenger, sender)
	    if (sender->wait_reason == MESSENGER_WAIT_TRANSFER_MESSAGE)
	      /* There is.  Transfer SENDER's message to MESSENGER.  */
	      {
		object_wait_queue_unlink (principal, sender);

		assert (messenger->blocked);
		messenger->blocked = 0;
		bool ret = messenger_message_transfer (principal,
						       messenger, sender,
						       true);
		assert (ret);

		break;
	      }

	  if (! sender)
	    /* There was no sender waiting.  */
	    {
	      if ((flags & VG_IPC_RECEIVE_NONBLOCKING))
		/* The receive phase is non-blocking.  */
		REPLY (EWOULDBLOCK);
	      else
		/* Unblock MESSENGER.  */
		messenger->blocked = 0;
	    }
	}

      if (! (flags & VG_IPC_SEND))
	/* No send phase.  */
	{
	  if ((flags & VG_IPC_RETURN))
	    /* But a return phase.  */
	    REPLY (0);

	  continue;
	}

      /* Send phase.  */

      if ((flags & VG_IPC_SEND_INLINE))
	label = inline_word1;

      principal = activity;
      struct vg_cap principal_cap;
      if (! VG_ADDR_IS_VOID (send_activity))
	{
	  /* We need the cap below, otherwise, we could just use
	     OBJECT.  */
	  principal_cap = CAP (&thread->aspace,
			       send_activity, vg_cap_activity, false);
	  principal = (struct activity *) vg_cap_to_object (principal,
							 &principal_cap);
	  if (! principal)
	    {
	      DEBUG (4, "Dangling pointer at " VG_ADDR_FMT,
		     VG_ADDR_PRINTF (send_activity));
	      REPLY (ENOENT);
	    }
	}
      else
	{
	  principal_cap = thread->activity;
	  principal = activity;
	}

      struct messenger *source
	= (struct messenger *) OBJECT (&thread->aspace,
				       send_messenger, vg_cap_messenger,
				       true, NULL);
      if (unlikely (! source))
	{
	  DEBUG (0, "Source not valid.");
	  REPLY (ENOENT);
	}

      if (! (flags & VG_IPC_SEND_INLINE)
	  && unlikely (! VG_ADDR_IS_VOID (send_buf)))
	source->buffer = CAP (&thread->aspace, send_buf, vg_cap_page, true);

      if (unlikely ((flags & VG_IPC_SEND_SET_THREAD_TO_CALLER)))
	source->thread = object_to_cap ((struct vg_object *) thread);

      if (unlikely ((flags & VG_IPC_SEND_SET_ASROOT_TO_CALLERS)))
	source->as_root = thread->aspace;

      source->activate_on_send = (flags & VG_IPC_SEND_ACTIVATE);

      bool target_writable = true;
      struct vg_object *target;
      /* We special case VOID to mean the current thread.  */
      if (VG_ADDR_IS_VOID (target_messenger))
	target = (struct vg_object *) thread;
      else
	target = OBJECT (&thread->aspace, target_messenger, -1, false,
			 &target_writable);
      if (! target)
	{
	  DEBUG (0, "Target not valid.");
	  REPLY (ENOENT);
	}

      if (object_type (target) == vg_cap_messenger && ! target_writable)
	/* TARGET is a weak reference to a messenger.  Forward the
	   message.  */
	{
	  DEBUG (5, "IPC: " VG_OID_FMT " -> " VG_OID_FMT,
		 VG_OID_PRINTF (object_oid ((struct vg_object *) source)),
		 VG_OID_PRINTF (object_oid ((struct vg_object *) target)));

	  if ((flags & VG_IPC_SEND_INLINE))
	    {
	      source->out_of_band = false;
	      source->inline_words[0] = inline_word1;
	      source->inline_words[1] = inline_word2;
	      source->inline_caps[0] = inline_cap;

	      if ((flags & VG_IPC_SEND_INLINE_WORD1)
		  && (flags & VG_IPC_SEND_INLINE_WORD2))
		source->inline_word_count = 2;
	      else if ((flags & VG_IPC_SEND_INLINE_WORD1))
		source->inline_word_count = 1;
	      else
		source->inline_word_count = 0;

	      if ((flags & VG_IPC_SEND_INLINE_CAP1))
		source->inline_cap_count = 1;
	      else
		source->inline_cap_count = 0;
	    }
	  else
	    source->out_of_band = true;

	  if (messenger_message_transfer (principal,
					  (struct messenger *) target,
					  source,
					  ! (flags & VG_IPC_SEND_NONBLOCKING)))
	    /* The messenger has been enqueued.  */
	    {
	      if ((flags & VG_IPC_RETURN))
		REPLY (0);
	      continue;
	    }
	  else
	    REPLY (ETIMEDOUT);
	}

      /* TARGET designates a kernel implemented object.  Implement
	 it.  */

      /* The reply messenger (if any).  */
      struct messenger *reply = NULL;

      /* We are now so far that we should not reply to the caller but
	 to TARGET.  Set up our handy REPLY macro to do so.  */
#undef REPLY
  /* Send a reply indicating that an error with the error code ERR_.
     Go to the start of the server loop.  */
#define REPLY(err_)						\
      do							\
	{							\
	  if (err_)						\
	    DEBUG (0, DEBUG_BOLD ("Returning error %d"), err_);	\
	  if (reply)						\
	    if (rpc_error_reply (principal, reply, err_))	\
	      DEBUG (0, DEBUG_BOLD ("Failed to send reply"));	\
	  goto out;						\
	}							\
      while (0)


      struct vg_message *message;
      if ((flags & VG_IPC_SEND_INLINE))
	{
	  message = reply_buffer;
	  vg_message_clear (message);
	  if ((flags & VG_IPC_SEND_INLINE_WORD1))
	    vg_message_append_word (message, inline_word1);
	  if ((flags & VG_IPC_SEND_INLINE_WORD2))
	    vg_message_append_word (message, inline_word2);
	  if ((flags & VG_IPC_SEND_INLINE_CAP1))
	    vg_message_append_cap (message, inline_cap);
	}
      else
	{
	  if (source->buffer.type != vg_cap_page)
	    {
	      DEBUG (0, "Sender user-buffer has wrong type: %s",
		     vg_cap_type_string (source->buffer.type));
	      REPLY (EINVAL);
	    }
	  message = (struct vg_message *) vg_cap_to_object (principal,
							 &source->buffer);
	  if (! message)
	    {
	      DEBUG (0, "Sender user-buffer has wrong type: %s",
		     vg_cap_type_string (source->buffer.type));
	      REPLY (EINVAL);
	    }
	}

      label = vg_message_word (message, 0);

      do_debug (5)
	{
	  DEBUG (0, "");
	  vg_message_dump (message);
	}

      /* Extract the reply messenger (if any).  */
      if (vg_message_cap_count (message) > 0)
	/* We only look for a messenger here.  We know that any reply
	   that a kernel object generates that is sent to a kernel
	   object will just result in a discarded EINVAL.  */
	reply = (struct messenger *)
	  OBJECT (&thread->aspace,
		  vg_message_cap (message, 0),
		  vg_cap_rmessenger, false, NULL);

      /* There are a number of methods that look up an object relative
	 to the invoked object.  Generate an appropriate root for
	 them.  */
      struct vg_cap target_root_cap;
      struct vg_cap *target_root;
      if (likely (target == (struct vg_object *) thread))
	target_root = &thread->aspace;
      else if (object_type (target) == vg_cap_thread)
	target_root = &((struct thread *) target)->aspace;
      else
	{
	  target_root_cap = object_to_cap (target);
	  target_root = &target_root_cap;
	}

      DEBUG (4, VG_OID_FMT " %s(%llx) -> " VG_OID_FMT " %s(%llx)",
	     VG_OID_PRINTF (object_oid ((struct vg_object *) source)),
	     vg_cap_type_string (object_type ((struct vg_object *) source)),
	     source->id,
	     VG_OID_PRINTF (object_oid ((struct vg_object *) target)),
	     vg_cap_type_string (object_type (target)),
	     object_type (target) == vg_cap_messenger
	     ? ((struct messenger *) target)->id : 0);
      if (reply)
	DEBUG (4, "reply to: " VG_OID_FMT "(%llx)",
	       VG_OID_PRINTF (object_oid ((struct vg_object *) reply)),
	       reply->id);

      switch (label)
	{
	case VG_write:
	  {
	    struct io_buffer buffer;
	    err = vg_write_send_unmarshal (message, &buffer, NULL);
	    if (! err)
	      {
		int i;
		for (i = 0; i < buffer.len; i ++)
		  putchar (buffer.data[i]);
	      }

	    vg_write_reply (activity, reply);
	    break;
	  }
	case VG_read:
	  {
	    int max;
	    err = vg_read_send_unmarshal (message, &max, NULL);
	    if (err)
	      {
		DEBUG (0, "Read error!");
		REPLY (EINVAL);
	      }

	    struct io_buffer buffer;
	    buffer.len = 0;

	    if (max > 0)
	      {
		buffer.len = 1;
		buffer.data[0] = getchar ();
	      }

	    vg_read_reply (activity, reply, buffer);
	    break;
	  }

	case VG_fault:
	  {
	    uintptr_t start;
	    int max;

	    err = vg_fault_send_unmarshal (message, &start, &max, NULL);
	    if (err)
	      REPLY (err);

	    DEBUG (4, "(%p, %d)", (void *) start, max);

	    start &= ~(PAGESIZE - 1);

	    vg_fault_reply (activity, reply, 0);
	    int limit = (L4_NUM_MRS - 1
			 - l4_untyped_words (l4_msg_msg_tag (msg)))
	      * sizeof (uintptr_t) / sizeof (l4_map_item_t);
	    if (max > limit)
	      max = limit;

	    l4_map_item_t map_items[max];
	    int count = 0;
	    for (count = 0; count < max; count ++, start += PAGESIZE)
	      {
		struct vg_cap cap;
		bool writable;
		cap = as_object_lookup_rel (activity, &thread->aspace,
					    vg_addr_chop (VG_PTR_TO_ADDR (start),
						       PAGESIZE_LOG2),
					    vg_cap_rpage, &writable);

		if (cap.type != vg_cap_page && cap.type != vg_cap_rpage)
		  break;

		if (! writable && cap.discardable)
		  cap.discardable = false;

		struct vg_object *page = vg_cap_to_object (activity, &cap);
		if (! page)
		  break;

		object_desc_unmap (object_to_object_desc (page));
		object_to_object_desc (page)->mapped = true;

		int access = L4_FPAGE_READABLE;
		if (writable)
		  access |= L4_FPAGE_WRITABLE;

		l4_fpage_t fpage = l4_fpage ((uintptr_t) page, PAGESIZE);
		fpage = l4_fpage_add_rights (fpage, access);
		map_items[count] = l4_map_item (fpage, start);

		DEBUG (4, "Prefault %d: " DEBUG_BOLD ("%x") " <- %x/%x %s",
		       count + 1, start,
		       l4_address (fpage), l4_rights (fpage),
		       vg_cap_type_string (cap.type));
	      }

	    if (count > 0)
	      DEBUG (4, "Prefaulted %d pages (%d/%d)", count,
		     l4_untyped_words (l4_msg_msg_tag (msg)),
		     l4_typed_words (l4_msg_msg_tag (msg)));

	    vg_fault_reply (activity, reply, count);
	    int i;
	    for (i = 0; i < count; i ++)
	      l4_msg_append_map_item (msg, map_items[i]);

	    break;
	  }

	case VG_folio_alloc:
	  {
	    if (object_type (target) != vg_cap_activity_control)
	      {
		DEBUG (0, "target " VG_ADDR_FMT " not an activity but a %s",
		       VG_ADDR_PRINTF (target_messenger),
		       vg_cap_type_string (object_type (target)));
		REPLY (EINVAL);
	      }

	    struct activity *activity = (struct activity *) target;

	    struct vg_folio_policy policy;
	    err = vg_folio_alloc_send_unmarshal (message, &policy, NULL);
	    if (err)
	      REPLY (err);

	    DEBUG (4, "(" VG_ADDR_FMT ")", VG_ADDR_PRINTF (target_messenger));

	    struct vg_folio *folio = folio_alloc (activity, policy);
	    if (! folio)
	      REPLY (ENOMEM);

	    vg_folio_alloc_reply (principal, reply,
				  object_to_cap ((struct vg_object *) folio));
	    break;
	  }

	case VG_folio_free:
	  {
	    if (object_type (target) != vg_cap_folio)
	      REPLY (EINVAL);

	    struct vg_folio *folio = (struct vg_folio *) target;

	    err = vg_folio_free_send_unmarshal (message, NULL);
	    if (err)
	      REPLY (err);

	    DEBUG (4, "(" VG_ADDR_FMT ")", VG_ADDR_PRINTF (target_messenger));

	    folio_free (principal, folio);

	    vg_folio_free_reply (activity, reply);
	    break;
	  }

	case VG_folio_object_alloc:
	  {
	    if (object_type (target) != vg_cap_folio)
	      REPLY (EINVAL);

	    struct vg_folio *folio = (struct vg_folio *) target;

	    uint32_t idx;
	    uint32_t type;
	    struct vg_object_policy policy;
	    uintptr_t return_code;

	    err = vg_folio_object_alloc_send_unmarshal (message,
							&idx, &type, &policy,
							&return_code, NULL);
	    if (err)
	      REPLY (err);

	    DEBUG (4, "(" VG_ADDR_FMT ", %d (" VG_ADDR_FMT "), %s, (%s, %d), %d)",
		   VG_ADDR_PRINTF (target_messenger), idx,
		   vg_addr_depth (target_messenger) + VG_FOLIO_OBJECTS_LOG2
		   <= VG_ADDR_BITS
		   ? VG_ADDR_PRINTF (vg_addr_extend (target_messenger,
					       idx, VG_FOLIO_OBJECTS_LOG2))
		   : VG_ADDR_PRINTF (VG_ADDR_VOID),
		   vg_cap_type_string (type),
		   policy.discardable ? "discardable" : "precious",
		   policy.priority,
		   return_code);

	    if (idx >= VG_FOLIO_OBJECTS)
	      REPLY (EINVAL);

	    if (! (VG_CAP_TYPE_MIN <= type && type <= VG_CAP_TYPE_MAX))
	      REPLY (EINVAL);

	    struct vg_cap cap;
	    cap = folio_object_alloc (principal,
				      folio, idx, type, policy, return_code);

	    struct vg_cap weak = cap;
	    weak.type = vg_cap_type_weaken (cap.type);

	    vg_folio_object_alloc_reply (activity, reply, cap, weak);
	    break;
	  }

	case VG_folio_policy:
	  {
	    if (object_type (target) != vg_cap_folio)
	      REPLY (EINVAL);

	    struct vg_folio *folio = (struct vg_folio *) target;

	    uintptr_t flags;
	    struct vg_folio_policy in, out;

	    err = vg_folio_policy_send_unmarshal (message, &flags, &in, NULL);
	    if (err)
	      REPLY (err);

	    DEBUG (4, "(" VG_ADDR_FMT ", %d)",
		   VG_ADDR_PRINTF (target_messenger), flags);

	    folio_policy (principal, folio, flags, in, &out);

	    vg_folio_policy_reply (activity, reply, out);
	    break;
	  }

	case VG_cap_copy:
	  {
	    vg_addr_t source_as_addr;
	    vg_addr_t source_addr;
	    struct vg_cap source;
	    vg_addr_t target_as_addr;
	    vg_addr_t target_addr;
	    uint32_t flags;
	    struct vg_cap_properties properties;

	    err = vg_cap_copy_send_unmarshal (message,
					      &target_addr, 
					      &source_as_addr, &source_addr,
					      &flags, &properties, NULL);
	    if (err)
	      REPLY (err);

	    DEBUG (4, "(" VG_ADDR_FMT "@" VG_ADDR_FMT " <- "
		   VG_ADDR_FMT "@" VG_ADDR_FMT ", %s%s%s%s%s%s, %s/%d %lld/%d %d/%d",

		   VG_ADDR_PRINTF (target_messenger), VG_ADDR_PRINTF (target_addr),
		   VG_ADDR_PRINTF (source_as_addr), VG_ADDR_PRINTF (source_addr),
		   
		   VG_CAP_COPY_COPY_ADDR_TRANS_SUBPAGE & flags
		   ? "copy subpage/" : "",
		   VG_CAP_COPY_COPY_ADDR_TRANS_GUARD & flags
		   ? "copy trans guard/" : "",
		   VG_CAP_COPY_COPY_SOURCE_GUARD & flags
		   ? "copy src guard/" : "",
		   VG_CAP_COPY_WEAKEN & flags ? "weak/" : "",
		   VG_CAP_COPY_DISCARDABLE_SET & flags ? "discardable/" : "",
		   VG_CAP_COPY_PRIORITY_SET & flags ? "priority" : "",

		   properties.policy.discardable ? "discardable" : "precious",
		   properties.policy.priority,
		   VG_CAP_ADDR_TRANS_GUARD (properties.addr_trans),
		   VG_CAP_ADDR_TRANS_GUARD_BITS (properties.addr_trans),
		   VG_CAP_ADDR_TRANS_SUBPAGE (properties.addr_trans),
		   VG_CAP_ADDR_TRANS_SUBPAGES (properties.addr_trans));

	    struct vg_cap *target;
	    target = SLOT (target_root, target_addr);

	    target_root = ROOT (source_as_addr);
	    source = CAP (target_root, source_addr, -1, false);

	    if ((flags & ~(VG_CAP_COPY_COPY_ADDR_TRANS_SUBPAGE
			   | VG_CAP_COPY_COPY_ADDR_TRANS_GUARD
			   | VG_CAP_COPY_COPY_SOURCE_GUARD
			   | VG_CAP_COPY_WEAKEN
			   | VG_CAP_COPY_DISCARDABLE_SET
			   | VG_CAP_COPY_PRIORITY_SET)))
	      REPLY (EINVAL);

	    DEBUG (4, "(target: (" VG_ADDR_FMT ") " VG_ADDR_FMT ", "
		   "source: (" VG_ADDR_FMT ") " VG_ADDR_FMT ", "
		   "%s|%s, %s {%llx/%d %d/%d})",
		   VG_ADDR_PRINTF (target_as_addr), VG_ADDR_PRINTF (target_addr),
		   VG_ADDR_PRINTF (source_as_addr), VG_ADDR_PRINTF (source_addr),
		   flags & VG_CAP_COPY_COPY_ADDR_TRANS_GUARD ? "copy trans"
		   : (flags & VG_CAP_COPY_COPY_SOURCE_GUARD ? "source"
		      : "preserve"),
		   flags & VG_CAP_COPY_COPY_ADDR_TRANS_SUBPAGE ? "copy"
		   : "preserve",
		   flags & VG_CAP_COPY_WEAKEN ? "weaken" : "no weaken",
		   VG_CAP_ADDR_TRANS_GUARD (properties.addr_trans),
		   VG_CAP_ADDR_TRANS_GUARD_BITS (properties.addr_trans),
		   VG_CAP_ADDR_TRANS_SUBPAGE (properties.addr_trans),
		   VG_CAP_ADDR_TRANS_SUBPAGES (properties.addr_trans));

	    bool r = vg_cap_copy_x (principal,
				 VG_ADDR_VOID, target, VG_ADDR_VOID,
				 VG_ADDR_VOID, source, VG_ADDR_VOID,
				 flags, properties);
	    if (! r)
	      REPLY (EINVAL);

	    if ((flags & (VG_CAP_COPY_DISCARDABLE_SET | VG_CAP_COPY_PRIORITY_SET)))
	      /* The caller changed the policy.  Also change it on the
		 object.  */
	      {
		struct vg_object *object = cap_to_object_soft (principal,
							    target);
		if (object)
		  {
		    struct object_desc *desc
		      = object_to_object_desc (object);

		    struct vg_object_policy p = desc->policy;

		    /* XXX: This should only be allowed if TARGET
		       grants writable access to the object.  */
		    if ((flags & VG_CAP_COPY_DISCARDABLE_SET))
		      p.discardable = properties.policy.discardable;

		    /* Only the current claimant can set the
		       priority.  */
		    if ((flags & VG_CAP_COPY_PRIORITY_SET)
			&& desc->activity == principal)
		      p.priority = properties.policy.priority;

		    object_desc_claim (desc->activity, desc, p, true);
		  }
	      }

	    vg_cap_copy_reply (activity, reply);

#if 0
	    /* XXX: Surprisingly, it appears that this may be
	       more expensive than just faulting the pages
	       normally.  This needs more investivation.  */
	    if (VG_ADDR_IS_VOID (target_as_addr)
		&& vg_cap_types_compatible (target->type, vg_cap_page)
		&& VG_CAP_GUARD_BITS (target) == 0
		&& vg_addr_depth (target_addr) == VG_ADDR_BITS - PAGESIZE_LOG2)
	      /* The target address space is the caller's.  The target
		 object appears to be a page.  It seems to be
		 installed at a point where it would appear in the
		 hardware address space.  If this is really the case,
		 then we can map it now and save a fault later.  */
	      {
		profile_region ("vg_cap_copy-prefault");

		struct vg_cap cap = *target;
		if (target->type == vg_cap_rpage)
		  cap.discardable = false;

		struct vg_object *page = cap_to_object_soft (principal, &cap);
		if (page)
		  {
		    object_to_object_desc (page)->mapped = true;

		    l4_fpage_t fpage
		      = l4_fpage ((uintptr_t) page, PAGESIZE);
		    fpage = l4_fpage_add_rights (fpage, L4_FPAGE_READABLE);
		    if (cap.type == vg_cap_page)
		      fpage = l4_fpage_add_rights (fpage,
						   L4_FPAGE_WRITABLE);

		    uintptr_t page_addr = vg_addr_prefix (target_addr);

		    l4_map_item_t map_item = l4_map_item (fpage, page_addr);

		    l4_msg_append_map_item (msg, map_item);
		  }

		profile_region_end ();
	      }
#endif

	    break;
	  }

	case VG_cap_rubout:
	  {
	    vg_addr_t addr;

	    err = vg_cap_rubout_send_unmarshal (message, &addr, NULL);
	    if (err)
	      REPLY (err);

	    DEBUG (4, VG_ADDR_FMT "@" VG_ADDR_FMT,
		   VG_ADDR_PRINTF (target_messenger),
		   VG_ADDR_PRINTF (addr));

	    /* We don't look up the argument directly as we need to
	       respect any subpage specification for cappages.  */
	    struct vg_cap *slot = SLOT (target_root, addr);

	    cap_shootdown (principal, slot);

	    memset (target, 0, sizeof (*slot));

	    vg_cap_rubout_reply (activity, reply);
	    break;
	  }

	case VG_cap_read:
	  {
	    vg_addr_t cap_addr;

	    err = vg_cap_read_send_unmarshal (message, &cap_addr, NULL);
	    if (err)
	      REPLY (err);

	    DEBUG (4, VG_ADDR_FMT "@" VG_ADDR_FMT,
		   VG_ADDR_PRINTF (target_messenger), VG_ADDR_PRINTF (cap_addr));

	    struct vg_cap cap = CAP (target_root, cap_addr, -1, false);
	    /* Even if CAP.TYPE is not void, the cap may not designate
	       an object.  Looking up the object will set CAP.TYPE to
	       vg_cap_void if this is the case.  */
	    if (cap.type != vg_cap_void)
	      vg_cap_to_object (principal, &cap);

	    vg_cap_read_reply (activity, reply, cap.type,
			       VG_CAP_PROPERTIES_GET (cap));
	    break;
	  }

	case VG_object_discarded_clear:
	  {
	    vg_addr_t object_addr;

	    err = vg_object_discarded_clear_send_unmarshal
	      (message, &object_addr, NULL);
	    if (err)
	      REPLY (err);

	    DEBUG (4, VG_ADDR_FMT, VG_ADDR_PRINTF (object_addr));

	    /* We can't look up the object use OBJECT as object_lookup
	       returns NULL if the object's discardable bit is set!
	       Instead, we lookup the capability, find the object's
	       folio and then clear its discarded bit.  */
	    struct vg_cap cap = CAP (&thread->aspace, object_addr, -1, true);
	    if (cap.type == vg_cap_void)
	      REPLY (ENOENT);
	    if (vg_cap_type_weak_p (cap.type))
	      REPLY (EPERM);

	    int idx = (cap.oid % (1 + VG_FOLIO_OBJECTS)) - 1;
	    vg_oid_t foid = cap.oid - idx - 1;

	    struct vg_folio *folio = (struct vg_folio *)
	      object_find (activity, foid, VG_OBJECT_POLICY_VOID);

	    if (folio_object_version (folio, idx) != cap.version)
	      REPLY (ENOENT);

	    bool was_discarded = folio_object_discarded (folio, idx);
	    folio_object_discarded_set (folio, idx, false);

	    vg_object_discarded_clear_reply (activity, reply);

#if 0
	    /* XXX: Surprisingly, it appears that this may be more
	       expensive than just faulting the pages normally.  This
	       needs more investivation.  */
	    if (was_discarded
		&& cap.type == vg_cap_page
		&& VG_CAP_GUARD_BITS (&cap) == 0
		&& (vg_addr_depth (object_addr) == VG_ADDR_BITS - PAGESIZE_LOG2))
	      /* The target object was discarded, appears to be a page
		 and seems to be installed at a point where it would
		 appear in the hardware address space.  If this is
		 really the case, then we can map it now and save a
		 fault later.  */
	      {
		profile_region ("object_discard-prefault");

		struct vg_object *page = vg_cap_to_object (principal, &cap);
		if (page)
		  {
		    object_to_object_desc (page)->mapped = true;

		    l4_fpage_t fpage = l4_fpage ((uintptr_t) page, PAGESIZE);
		    fpage = l4_fpage_add_rights (fpage,
						 L4_FPAGE_READABLE
						 | L4_FPAGE_WRITABLE);

		    uintptr_t page_addr = vg_addr_prefix (object_addr);

		    l4_map_item_t map_item = l4_map_item (fpage, page_addr);

		    l4_msg_append_map_item (msg, map_item);

		    DEBUG (4, "Prefaulting "VG_ADDR_FMT"(%x) <- %p (%x/%x/%x)",
			   VG_ADDR_PRINTF (object_addr), page_addr,
			   page, l4_address (fpage), l4_size (fpage),
			   l4_rights (fpage));
		  }

		profile_region_end ();
	      }
#endif

	    break;
	  }

	case VG_object_discard:
	  {
	    err = vg_object_discard_send_unmarshal (message, NULL);
	    if (err)
	      REPLY (err);

	    DEBUG (4, VG_ADDR_FMT, VG_ADDR_PRINTF (target_messenger));

	    struct vg_folio *folio = objects_folio (principal, target);

	    folio_object_content_set (folio,
				      objects_folio_offset (target), false);

	    vg_object_discard_reply (activity, reply);
	    break;
	  }

	case VG_object_status:
	  {
	    bool clear;
	    err = vg_object_status_send_unmarshal (message, &clear, NULL);
	    if (err)
	      REPLY (err);

	    DEBUG (4, VG_ADDR_FMT ", %sclear",
		   VG_ADDR_PRINTF (target_messenger), clear ? "" : "no ");

	    struct object_desc *desc = object_to_object_desc (target);
	    uintptr_t status = (desc->user_referenced ? vg_object_referenced : 0)
	      | (desc->user_dirty ? vg_object_dirty : 0);

	    if (clear)
	      {
		desc->user_referenced = 0;
		desc->user_dirty = 0;
	      }

	    vg_object_status_reply (activity, reply, status);
	    break;
	  }

	case VG_object_name:
	  {
	    struct vg_object_name name;
	    err = vg_object_name_send_unmarshal (message, &name, NULL);

	    if (object_type (target) == vg_cap_activity_control)
	      {
		struct activity *a = (struct activity *) target;

		memcpy (a->name.name, name.name, sizeof (name));
		a->name.name[sizeof (a->name.name) - 1] = 0;
	      }
	    else if (object_type (target) == vg_cap_thread)
	      {
		struct thread *t = (struct thread *) target;

		memcpy (t->name.name, name.name, sizeof (name));
		t->name.name[sizeof (t->name.name) - 1] = 0;
	      }

	    vg_object_name_reply (activity, reply);
	    break;
	  }

	case VG_thread_exregs:
	  {
	    if (object_type (target) != vg_cap_thread)
	      REPLY (EINVAL);
	    struct thread *t = (struct thread *) target;

	    struct vg_thread_exregs_in in;
	    uintptr_t control;
	    vg_addr_t aspace_addr;
	    vg_addr_t activity_addr;
	    vg_addr_t utcb_addr;
	    vg_addr_t exception_messenger_addr;
	    err = vg_thread_exregs_send_unmarshal
	      (message, &control, &in,
	       &aspace_addr, &activity_addr, &utcb_addr,
	       &exception_messenger_addr,
	       NULL);
	    if (err)
	      REPLY (err);

	    int d = 4;
	    DEBUG (d, "%s%s" VG_ADDR_FMT "(%x): %s%s%s%s %s%s%s%s %s%s%s %s%s",
		   t->name.name[0] ? t->name.name : "",
		   t->name.name[0] ? ": " : "",
		   VG_ADDR_PRINTF (target_messenger), t->tid,
		   (control & VG_EXREGS_SET_UTCB) ? "U" : "-",
		   (control & VG_EXREGS_SET_EXCEPTION_MESSENGER) ? "E" : "-",
		   (control & VG_EXREGS_SET_ASPACE) ? "R" : "-",
		   (control & VG_EXREGS_SET_ACTIVITY) ? "A" : "-",
		   (control & VG_EXREGS_SET_SP) ? "S" : "-",
		   (control & VG_EXREGS_SET_IP) ? "I" : "-",
		   (control & VG_EXREGS_SET_EFLAGS) ? "F" : "-",
		   (control & VG_EXREGS_SET_USER_HANDLE) ? "U" : "-",
		   (control & _L4_XCHG_REGS_CANCEL_RECV) ? "R" : "-",
		   (control & _L4_XCHG_REGS_CANCEL_SEND) ? "S" : "-",
		   (control & _L4_XCHG_REGS_CANCEL_IPC) ? "I" : "-",
		   (control & _L4_XCHG_REGS_HALT) ? "H" : "-",
		   (control & _L4_XCHG_REGS_SET_HALT) ? "Y" : "N");

	    if ((control & VG_EXREGS_SET_UTCB))
	      DEBUG (d, "utcb: " VG_ADDR_FMT, VG_ADDR_PRINTF (utcb_addr));
	    if ((control & VG_EXREGS_SET_EXCEPTION_MESSENGER))
	      DEBUG (d, "exception messenger: " VG_ADDR_FMT,
		     VG_ADDR_PRINTF (exception_messenger_addr));
	    if ((control & VG_EXREGS_SET_ASPACE))
	      DEBUG (d, "aspace: " VG_ADDR_FMT, VG_ADDR_PRINTF (aspace_addr));
	    if ((control & VG_EXREGS_SET_ACTIVITY))
	      DEBUG (d, "activity: " VG_ADDR_FMT, VG_ADDR_PRINTF (activity_addr));
	    if ((control & VG_EXREGS_SET_SP))
	      DEBUG (d, "sp: %p", (void *) in.sp);
	    if ((control & VG_EXREGS_SET_IP))
	      DEBUG (d, "ip: %p", (void *) in.ip);
	    if ((control & VG_EXREGS_SET_EFLAGS))
	      DEBUG (d, "eflags: %p", (void *) in.eflags);
	    if ((control & VG_EXREGS_SET_USER_HANDLE))
	      DEBUG (d, "user_handle: %p", (void *) in.user_handle);

	    struct vg_cap aspace = VG_CAP_VOID;
	    if ((VG_EXREGS_SET_ASPACE & control))
	      aspace = CAP (&thread->aspace, aspace_addr, -1, false);

	    struct vg_cap a = VG_CAP_VOID;
	    if ((VG_EXREGS_SET_ACTIVITY & control))
	      {
		/* XXX: Remove this hack... */
		if (VG_ADDR_IS_VOID (activity_addr))
		  a = thread->activity;
		else
		  a = CAP (&thread->aspace,
			   activity_addr, vg_cap_activity, false);
	      }

	    struct vg_cap utcb = VG_CAP_VOID;
	    if ((VG_EXREGS_SET_UTCB & control))
	      utcb = CAP (&thread->aspace, utcb_addr, vg_cap_page, true);

	    struct vg_cap exception_messenger = VG_CAP_VOID;
	    if ((VG_EXREGS_SET_EXCEPTION_MESSENGER & control))
	      exception_messenger
		= CAP (&thread->aspace, exception_messenger_addr,
		       vg_cap_rmessenger, false);

	    struct vg_cap aspace_out = thread->aspace;
	    struct vg_cap activity_out = thread->activity;
	    struct vg_cap utcb_out = thread->utcb;
	    struct vg_cap exception_messenger_out = thread->exception_messenger;

	    struct vg_thread_exregs_out out;
	    out.sp = in.sp;
	    out.ip = in.ip;
	    out.eflags = in.eflags;
	    out.user_handle = in.user_handle;

	    err = thread_exregs (principal, t, control,
				 aspace, in.aspace_cap_properties_flags,
				 in.aspace_cap_properties,
				 a, utcb, exception_messenger,
				 &out.sp, &out.ip,
				 &out.eflags, &out.user_handle);
	    if (err)
	      REPLY (err);

	    vg_thread_exregs_reply (activity, reply, out,
				    aspace_out, activity_out,
				    utcb_out, exception_messenger_out);

	    break;
	  }

	case VG_thread_id:
	  {
	    if (object_type (target) != vg_cap_thread)
	      REPLY (EINVAL);
	    struct thread *t = (struct thread *) target;

	    err = vg_thread_id_send_unmarshal (message, NULL);
	    if (err)
	      REPLY (err);

	    vg_thread_id_reply (activity, reply, t->tid);
	    break;	    
	  }

	case VG_object_reply_on_destruction:
	  {
	    err = vg_object_reply_on_destruction_send_unmarshal (message,
								 NULL);
	    if (err)
	      REPLY (err);

	    DEBUG (4, VG_ADDR_FMT, VG_ADDR_PRINTF (target_messenger));

	    reply->wait_reason = MESSENGER_WAIT_DESTROY;
	    object_wait_queue_enqueue (principal, target, reply);

	    break;
	  }

	case VG_activity_policy:
	  {
	    if (object_type (target) != vg_cap_activity_control)
	      {
		DEBUG (0, "expects an activity, not a %s",
		       vg_cap_type_string (object_type (target)));
		REPLY (EINVAL);
	      }
	    struct activity *activity = (struct activity *) target;

	    uintptr_t flags;
	    struct vg_activity_policy in;

	    err = vg_activity_policy_send_unmarshal (message, &flags, &in,
						     NULL);
	    if (err)
	      REPLY (err);

	    int d = 4;
	    DEBUG (d, "(%s) child: %s%s; sibling: %s%s; storage: %s",
		   target_writable ? "strong" : "weak",
		   (flags & VG_ACTIVITY_POLICY_CHILD_REL_PRIORITY_SET) ? "P" : "-",
		   (flags & VG_ACTIVITY_POLICY_CHILD_REL_WEIGHT_SET) ? "W" : "-",
		   (flags & VG_ACTIVITY_POLICY_SIBLING_REL_PRIORITY_SET)
		   ? "P" : "-",
		   (flags & VG_ACTIVITY_POLICY_SIBLING_REL_WEIGHT_SET) ? "W" : "-",
		   (flags & VG_ACTIVITY_POLICY_STORAGE_SET) ? "P" : "-");

	    if ((flags & VG_ACTIVITY_POLICY_CHILD_REL_PRIORITY_SET))
	      DEBUG (d, "Child priority: %d", in.child_rel.priority);
	    if ((flags & VG_ACTIVITY_POLICY_CHILD_REL_WEIGHT_SET))
	      DEBUG (d, "Child weight: %d", in.child_rel.weight);
	    if ((flags & VG_ACTIVITY_POLICY_SIBLING_REL_PRIORITY_SET))
	      DEBUG (d, "Sibling priority: %d", in.sibling_rel.priority);
	    if ((flags & VG_ACTIVITY_POLICY_SIBLING_REL_WEIGHT_SET))
	      DEBUG (d, "Sibling weight: %d", in.sibling_rel.weight);
	    if ((flags & VG_ACTIVITY_POLICY_STORAGE_SET))
	      DEBUG (d, "Storage: %d", in.folios);

	    if (! target_writable
		&& (flags & (VG_ACTIVITY_POLICY_STORAGE_SET
			     | VG_ACTIVITY_POLICY_SIBLING_REL_SET)))
	      REPLY (EPERM);

	    vg_activity_policy_reply (principal, reply, activity->policy);

	    if ((flags & (VG_ACTIVITY_POLICY_CHILD_REL_PRIORITY_SET
			  | VG_ACTIVITY_POLICY_CHILD_REL_WEIGHT_SET
			  | VG_ACTIVITY_POLICY_SIBLING_REL_PRIORITY_SET
			  | VG_ACTIVITY_POLICY_SIBLING_REL_WEIGHT_SET
			  | VG_ACTIVITY_POLICY_STORAGE_SET)))
	      {
		struct vg_activity_policy p = principal->policy;

		if ((flags & VG_ACTIVITY_POLICY_CHILD_REL_PRIORITY_SET))
		  p.child_rel.priority = in.child_rel.priority;
		if ((flags & VG_ACTIVITY_POLICY_CHILD_REL_WEIGHT_SET))
		  p.child_rel.weight = in.child_rel.weight;

		if ((flags & VG_ACTIVITY_POLICY_SIBLING_REL_PRIORITY_SET))
		  p.sibling_rel.priority = in.sibling_rel.priority;
		if ((flags & VG_ACTIVITY_POLICY_SIBLING_REL_WEIGHT_SET))
		  p.sibling_rel.weight = in.sibling_rel.weight;

		if ((flags & VG_ACTIVITY_POLICY_STORAGE_SET))
		  p.folios = in.folios;

		activity_policy_update (activity, p);
	      }

	    break;
	  }

	case VG_activity_info:
	  {
	    if (object_type (target) != vg_cap_activity_control)
	      REPLY (EINVAL);
	    struct activity *activity = (struct activity *) target;

	    uintptr_t flags;
	    uintptr_t until_period;

	    err = vg_activity_info_send_unmarshal (message,
						   &flags, &until_period,
						   NULL);
	    if (err)
	      REPLY (err);

	    int period = activity->current_period - 1;
	    if (period < 0)
	      period = (VG_ACTIVITY_STATS_PERIODS + 1) + period;

	    DEBUG (4, OBJECT_NAME_FMT ": %s%s%s(%d), "
		   "period: %d (current: %d)",
		   OBJECT_NAME_PRINTF ((struct vg_object *) activity),
		   flags & vg_activity_info_stats ? "stats" : "",
		   (flags == (vg_activity_info_pressure|vg_activity_info_stats))
		   ? ", " : "",
		   flags & vg_activity_info_pressure ? "pressure" : "",
		   flags,
		   until_period, activity->stats[period].period);
	    
	    if ((flags & vg_activity_info_stats)
		&& activity->stats[period].period > 0
		&& activity->stats[period].period >= until_period)
	      /* Return the available statistics.  */
	      {
		/* XXX: Only return valid stat buffers.  */
		struct vg_activity_info info;
		info.event = vg_activity_info_stats;

		int i;
		for (i = 0; i < VG_ACTIVITY_STATS_PERIODS; i ++)
		  {
		    period = activity->current_period - 1 - i;
		    if (period < 0)
		      period = (VG_ACTIVITY_STATS_PERIODS + 1) + period;

		    info.stats.stats[i] = activity->stats[period];
		  }

		info.stats.count = VG_ACTIVITY_STATS_PERIODS;

		vg_activity_info_reply (principal, reply, info);
	      }
	    else if (flags)
	      /* Queue thread on the activity.  */
	      {
		reply->wait_reason = MESSENGER_WAIT_ACTIVITY_INFO;
		reply->wait_reason_arg = flags;
		reply->wait_reason_arg2 = until_period;

		object_wait_queue_enqueue (principal, target, reply);
	      }
	    else
	      REPLY (EINVAL);

	    break;
	  }

	case VG_thread_activation_collect:
	  {
	    if (object_type (target) != vg_cap_thread)
	      REPLY (EINVAL);

	    err = vg_thread_activation_collect_send_unmarshal (message, NULL);
	    if (err)
	      REPLY (err);

	    thread_deliver_pending (principal, (struct thread *) target);

	    vg_thread_activation_collect_reply (principal, reply);
	    break;
	  }

	case VG_as_dump:
	  {
	    err = vg_as_dump_send_unmarshal (message, NULL);
	    if (err)
	      REPLY (err);

	    as_dump_from (principal, target_root, "");

	    vg_as_dump_reply (activity, reply);

	    break;
	  }

	case VG_futex:
	  {
	    /* Helper function to wake and requeue waiters.  */
	    int wake (int to_wake, struct vg_object *object1, int offset1,
		      int to_requeue, struct vg_object *object2, int offset2)
	    {
	      int count = 0;
	      struct messenger *m;

	      object_wait_queue_for_each (principal, object1, m)
		if (m->wait_reason == MESSENGER_WAIT_FUTEX
		    && m->wait_reason_arg == offset1)
		  /* Got a match.  */
		  {
		    if (count < to_wake)
		      {
			object_wait_queue_unlink (principal, m);

			debug (5, "Waking messenger");

			err = vg_futex_reply (principal, m, 0);
			if (err)
			  panic ("Error vg_futex waking: %d", err);

			count ++;

			if (count == to_wake && to_requeue == 0)
			  break;
		      }
		    else
		      {
			object_wait_queue_unlink (principal, m);

			m->wait_reason_arg = offset2;
			object_wait_queue_enqueue (principal, object2, m);

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

	    err = vg_futex_send_unmarshal (message,
					   &addr1, &op, &val1,
					   &timeout, &val2,
					   &addr2, &val3, NULL);
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

	      struct vg_object *page = vg_cap_to_object (principal, &thread->utcb);
	      if (page && object_type (page) == vg_cap_page)
		{
		  struct vg_utcb *utcb = (struct vg_utcb *) page;

		  if (utcb->activated_mode)
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

	    vg_addr_t addr = vg_addr_chop (VG_PTR_TO_ADDR (addr1), PAGESIZE_LOG2);
	    struct vg_object *object1 = OBJECT (&thread->aspace,
					     addr, vg_cap_page, true, NULL);
	    int offset1 = (uintptr_t) addr1 & (PAGESIZE - 1);
	    int *vaddr1 = (void *) object1 + offset1;

	    switch (op)
	      {
	      case FUTEX_WAIT:
		if (*vaddr1 != val1)
		  REPLY (EWOULDBLOCK);

		if (timeout)
		  panic ("Timeouts not yet supported");

		reply->wait_reason = MESSENGER_WAIT_FUTEX;
		reply->wait_reason_arg = offset1;

		object_wait_queue_enqueue (principal, object1, reply);

#ifndef NDEBUG
		futex_waiter_list_enqueue (&futex_waiters, reply);
#endif

		break;

	      case FUTEX_WAKE:
		/* Wake up VAL1 threads or, if there are less than
		   VAL1 blocked threads, wake up all of them.  Return
		   the number of threads woken up.  */

		if (val1 <= 0)
		  REPLY (EINVAL);

		int count = wake (val1, object1, offset1, 0, 0, 0);
		vg_futex_reply (activity, reply, count);
		break;

	      case FUTEX_WAKE_OP:
		addr = vg_addr_chop (VG_PTR_TO_ADDR (addr2), PAGESIZE_LOG2);
		struct vg_object *object2 = OBJECT (&thread->aspace,
						 addr, vg_cap_page, true, NULL);
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

		vg_futex_reply (activity, reply, 0);
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
		addr = vg_addr_chop (VG_PTR_TO_ADDR (addr2), PAGESIZE_LOG2);
		object2 = OBJECT (&thread->aspace, addr, vg_cap_page, true, NULL);
		offset2 = (uintptr_t) addr2 & (PAGESIZE - 1);

		count = wake (val1, object1, offset1,
			      val2.value, object2, offset2);
		vg_futex_reply (activity, reply, count);
		break;
	      }

	    break;
	  }

	case VG_messenger_id:
	  {
	    if (object_type (target) != vg_cap_messenger || ! target_writable)
	      REPLY (EINVAL);
	    struct messenger *m = (struct messenger *) target;

	    uint64_t id;
	    err = vg_messenger_id_send_unmarshal (message, &id, NULL);
	    if (err)
	      REPLY (EINVAL);

	    uint64_t old = m->id;
	    m->id = id;

	    vg_messenger_id_reply (principal, reply, old);

	    break;
	  }

	default:
	  /* XXX: Don't panic when running production code.  */
	  DEBUG (1, "Didn't handle message from %x.%x with label %d",
		 l4_thread_no (from), l4_version (from), label);
	}

      if ((flags & VG_IPC_RETURN))
	{
	  l4_msg_clear (msg);
	  l4_msg_put_word (msg, 0, 0);
	  l4_msg_set_untyped_words (msg, 1);
	  do_reply = 1;
	}
      
    out:;
    }

  /* Should never return.  */
  panic ("server_loop returned!");
}
