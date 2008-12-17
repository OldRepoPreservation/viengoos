/* messenger.c - Messenger object implementation.
   Copyright (C) 2008 Free Software Foundation, Inc.
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

#include <stdbool.h>
#include <errno.h>
#include <assert.h>
#include <viengoos/cap.h>
#include <hurd/as.h>

#include "messenger.h"
#include "object.h"
#include "thread.h"

/* When the kernel formulates relies, it does so in this buffer.  */
static char reply_message_data[PAGESIZE] __attribute__ ((aligned (PAGESIZE)));
struct vg_message *reply_buffer = (struct vg_message *) &reply_message_data[0];

#define MIN(x, y) ((x) < (y) ? (x) : (y))
#define MAX(x, y) ((x) > (y) ? (x) : (y))

#include <backtrace.h>

static bool
messenger_load_internal (struct activity *activity,
			 struct messenger *target,
			 struct messenger *source,
			 struct vg_message *smessage,
			 bool may_block)
{
  assert (object_type ((struct object *) target) == cap_messenger);
  if (source)
    assert (object_type ((struct object *) source) == cap_messenger);

  if (source)
    assert (! smessage);
  else
    assert (smessage);

  /* SOURCE should not already be blocked on another messenger.  */
  if (source)
    {
      assert (! source->wait_queue.next);
      assert (! source->wait_queue.prev);
    }

  if (unlikely (target->blocked))
    /* TARGET is blocked.  */
    {
      if (! may_block)
	{
	  debug (0, "Not enqueuing messenger: "
		 "target blocked and delivery marked as non-blocking.");
	  backtrace_print ();
	  return false;
	}

      /* Enqueue SOURCE on TARGET's wait queue.  */

      debug (0, "Target blocked.  Enqueuing sender.");

      assert (source);
      source->wait_reason = MESSENGER_WAIT_TRANSFER_MESSAGE;
      object_wait_queue_enqueue (activity, (struct object *) target, source);

      return true;
    }

  /* TARGET is not blocked.  Deliver the message.  */
  debug (5, "Delivering sender's message to target.");

  target->blocked = true;

  /* There are four combinations: the source can either have inline
     data or out-of-line data and the target can either have inline
     data or out-of-line data.  */

  struct vg_message *tmessage = NULL;

  void *sdata;
  void *tdata;
  int data_count;

  addr_t *saddrs;
  int saddr_count;
  addr_t *taddrs;
  int taddr_count;

  if (! source || source->out_of_band)
    /* Source data is in a buffer.  */
    {
      if (source)
	smessage = (struct vg_message *) cap_to_object (activity,
							&source->buffer);
      else
	assert (smessage);

      if (smessage)
	{
	  sdata = vg_message_data (smessage);
	  data_count = vg_message_data_count (smessage);

	  saddrs = vg_message_caps (smessage);
	  saddr_count = vg_message_cap_count (smessage);
	}
      else
	{
	  sdata = NULL;
	  data_count = 0;
	  saddrs = NULL;
	  saddr_count = 0;
	}
    }
  else
    /* Source data is inline.  */
    {
      assert (source);

      sdata = source->inline_words;
      data_count
	= sizeof (source->inline_words[0]) * source->inline_word_count;

      saddrs = source->inline_caps;
      saddr_count = source->inline_cap_count;
    }

  if (target->out_of_band)
    /* Target data is in a buffer.  */
    {
      tmessage = (struct vg_message *) cap_to_object (activity,
						      &target->buffer);
      if (tmessage)
	{
	  taddrs = vg_message_caps (tmessage);
	  taddr_count = vg_message_cap_count (tmessage);

	  /* Set the number of capabilities to the number in the
	     source message.  */
	  tmessage->cap_count = saddr_count;
	  tdata = vg_message_data (tmessage);
	  tmessage->data_count = data_count;
	}
      else
	{
	  tdata = NULL;
	  data_count = 0;

	  taddrs = NULL;
	  taddr_count = 0;
	}
    }
  else
    /* Target data is inline.  */
    {
      tdata = target->inline_words;
      data_count = MIN (data_count,
			sizeof (uintptr_t) * VG_MESSENGER_INLINE_WORDS);
      target->inline_word_count
	= (data_count + sizeof (uintptr_t) - 1) / sizeof (uintptr_t);

      taddrs = target->inline_caps;
      taddr_count = target->inline_cap_count;
    }

  do_debug (5)
    {
      if (smessage)
	{
	  debug (0, "Source: ");
	  vg_message_dump (smessage);
	}
      if (tmessage)
	{
	  debug (0, "Target: ");
	  vg_message_dump (tmessage);
	}
    }

  /* Copy the caps.  */
  int i;
  for (i = 0; i < MIN (saddr_count, taddr_count); i ++)
    {
      /* First get the target capability slot.  */
      bool twritable = true;

      struct cap *tcap = NULL;
      if (! ADDR_IS_VOID (taddrs[i]))
	{
	  as_slot_lookup_rel_use (activity, &target->as_root, taddrs[i],
				  ({
				    twritable = writable;
				    tcap = slot;
				  }));
	  if (! tcap || ! twritable)
	    debug (0, DEBUG_BOLD ("Target " ADDR_FMT " does not designate "
				  "a %svalid slot!"),
		   ADDR_PRINTF (taddrs[i]), twritable ? "writable " : "");
	}

      if (likely (tcap && twritable))
	/* We have a slot and it is writable.  Look up the source
	   capability.  */
	{
	  struct cap scap = CAP_VOID;
	  bool swritable = true;
	  if (source)
	    {
	      if (! ADDR_IS_VOID (saddrs[i]))
		scap = as_cap_lookup_rel (activity,
					  &source->as_root, saddrs[i],
					  -1, &swritable);
	    }
	  else
	    /* This is a kernel provided buffer.  In this case the
	       address is really a pointer to a capability.  */
	    if ((uintptr_t) saddrs[i].raw)
	      scap = * (struct cap *) (uintptr_t) saddrs[i].raw;

	  if (! swritable)
	    scap.type = cap_type_weaken (scap.type);

	  /* Shoot down the capability.  */
	  cap_shootdown (activity, tcap);

	  /* Preserve the address translator and policy.  */
	  struct cap_properties props = CAP_PROPERTIES_GET (*tcap);
	  *tcap = scap;
	  CAP_PROPERTIES_SET (tcap, props);

	  debug (5, ADDR_FMT " <- " CAP_FMT,
		 ADDR_PRINTF (taddrs[i]), CAP_PRINTF (tcap));
	}
      else
	taddrs[i] = ADDR_VOID;
    }
  if (i < MAX (taddr_count, saddr_count) && target->out_of_band && taddrs)
    /* Set the address of any non-transferred caps in the target to
       ADDR_VOID.  */
    memset (&taddrs[i], 0,
	    sizeof (taddrs[0]) * (MAX (taddr_count, saddr_count)) - i);

  /* Copy the data.  */
  memcpy (tdata, sdata, data_count);

  do_debug (5)
    if (tmessage)
      {
	debug (0, "Delivery: ");
	vg_message_dump (tmessage);
      }

  if (target->activate_on_receive)
    messenger_message_deliver (activity, target);
  else
    debug (0, "Not activing target.");

  if (source && source->activate_on_send)
    messenger_message_deliver (activity, source);

  return true;
}

bool
messenger_message_transfer (struct activity *activity,
			    struct messenger *target,
			    struct messenger *source,
			    bool may_block)
{
  return messenger_load_internal (activity, target, source, NULL, may_block);
}

bool
messenger_message_load (struct activity *activity,
			struct messenger *target,
			struct vg_message *message)
{
  return messenger_load_internal (activity, target, NULL, message, false);
}

bool
messenger_message_deliver (struct activity *activity,
			   struct messenger *messenger)
{
  assert (messenger->blocked);
  assert (! messenger->wait_queue_p);

  struct thread *thread
    = (struct thread *) cap_to_object (activity, &messenger->thread);
  if (! thread)
    {
      debug (0, "Messenger has no thread to activate!");
      return false;
    }

  if (object_type ((struct object *) thread) != cap_thread)
    {
      debug (0, "Messenger's thread cap does not designate a thread but a %s",
	     cap_type_string (object_type ((struct object *) thread)));
      return false;
    }

  return thread_activate (activity, thread, messenger, true);
}

void
messenger_unblock (struct activity *activity, struct messenger *messenger)
{
  if (! messenger->blocked)
    return;

  messenger->blocked = 0;

  struct messenger *m;
  object_wait_queue_for_each (activity, (struct object *) messenger, m)
    if (m->wait_reason == MESSENGER_WAIT_TRANSFER_MESSAGE)
      {
	object_wait_queue_unlink (activity, m);
	bool ret = messenger_message_transfer (activity, messenger, m, true);
	assert (ret);

	break;
      }
}

void
messenger_destroy (struct activity *activity, struct messenger *messenger)
{
  if (messenger->wait_queue_p)
    /* MESSENGER is attached to a wait queue.  Detach it.  */
    object_wait_queue_unlink (activity, messenger);
}
