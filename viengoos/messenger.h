/* messenger.h - Messenger buffer definitions.
   Copyright (C) 2008 Free Software Foundation, Inc.
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

#ifndef _MESSENGER_H
#define _MESSENGER_H 1

#include <stdint.h>
#include <string.h>
#include <assert.h>
#include <viengoos/cap.h>
#include <viengoos/messenger.h>
#include <viengoos/message.h>

#ifndef NDEBUG
#include "../viengoos/list.h"
#endif

/* Messenger may be enqueued on any object and for different reasons.
   The reason an object is enqueued is stored in the WAIT_REASON.
   These are the reasons.  */
enum
  {
    /* The messenger is blocked on an object wait for a futex.
       WAIT_REASON_ARG holds the byte offset in the object on which it
       is waiting.  */
    MESSENGER_WAIT_FUTEX,

    /* The messenger is blocked on an object waiting for the object to
       be destroyed.  */
    MESSENGER_WAIT_DESTROY,

    /* The messenger is blocked on an activity waiting for
       information.  The type of information is stored in
       wait_reason_arg.  The period in wait_reason_arg2.  */
    MESSENGER_WAIT_ACTIVITY_INFO,

    /* The messenger is trying to transfer a message to another
       messenger or to a thread.  */
    MESSENGER_WAIT_TRANSFER_MESSAGE,
  };

/* Messenger object.  */
struct messenger
{
  /* When this messenger is activated (that is, its contents are
     delivered or it receives a message), THREAD is activated.  This
     is settable from user space.  */
  struct vg_cap thread;

  /* The root of the address space in which capability addresses
     referenced in the message are resolved.  */
  struct vg_cap as_root;

  /* The message buffer.  */
  struct vg_cap buffer;

  /* The activity supplied by the sender of the message.  */
  struct vg_cap sender_activity;


  /* Whether the data is inline or out of line.  */
  bool out_of_band;

  /* The inline data.  */
  int inline_word_count;
  int inline_cap_count;

  /* Inline data.  */
  uintptr_t inline_words[VG_MESSENGER_INLINE_WORDS];
  vg_addr_t inline_caps[VG_MESSENGER_INLINE_CAPS];


  /* The buffer's version.  If USER_VERSION_MATCHING is true, a
     message can only be delivered if the user version in the
     capability used to designate the buffer matches the buffer's user
     version.  */
  uint64_t user_version;

  /* If the user version in the capability must match
     USER_VERSION. */
  bool user_version_matching;
  bool user_version_increment_on_delivery;


  /* If the buffer is blocked, no messages will be delivered.
     When a message is deliveried to this buffer, this is set to
     true.  */
  bool blocked;

  /* Activate thread when this messenger receives a message.  */
  bool activate_on_receive;
  /* Activate thread when this messenger sends a message.  */
  bool activate_on_send;

  /* The payload in the capability that was used to delivery the
     message.  This is only valid if this buffer contains an
     (undelivered) message.  */
  uint64_t protected_payload;

  /* The messenger's identifier.  */
  uint64_t id;


  /* The object the messenger is waiting on.  Only meaningful if
     WAIT_QUEUE_P is true.

     The list node used to connect a messenger to its target's
     sender's wait queue.

     Senders are arranged in a doubly-linked list.  The head points to
     the second element and the last element.  The last element points
     to the root and the second to last object.


             H ----> 1
             ^     //\
             |   /  ||
             ||/_   \/
             3 <===> 2

     Next pointers: H -> 1 -> 2 -> 3 -> H
     Previous pointers: 1 -> 3 -> 2 -> 1
   */
  struct
  {
    /* We don't need versioning as we automatically collect on object
       destruction.  */
    vg_oid_t next;
    vg_oid_t prev;
  } wait_queue;

  /* Whether the object is attached to a wait queue.  (This is
     different from the value of folio_object_wait_queue_p which
     specifies if there are objects on this thread's wait queue.)  */
  uint32_t wait_queue_p : 1;

  /* Whether this messenger is the head of the wait queue.  If so,
     WAIT_QUEUE.PREV designates the object.  */
  uint32_t wait_queue_head : 1;

  /* Whether this messenger is the tail of the wait queue.  If so,
     WAIT_QUEUE.NEXT designates the object.  */
  uint32_t wait_queue_tail : 1;


  /* Why the messenger is on a wait queue.  */
  uint32_t wait_reason : 27;
  /* Additional information about the reason.  */
  uint32_t wait_reason_arg;
  uint32_t wait_reason_arg2;

#ifndef NDEBUG
  /* Used for debugging futexes.  */
  struct list_node futex_waiter_node;
#endif
};

#ifndef NDEBUG
LIST_CLASS(futex_waiter, struct messenger, futex_waiter_node, true)
/* List of threads waiting on a futex.  */
extern struct futex_waiter_list futex_waiters;
#endif

/* When the kernel formulates relies, it does so in this buffer.  */
extern struct vg_message *reply_buffer;

/* Transfer SOURCE's message contents to TARGET.  If TARGET is blocked
   and MAY_BLOCK is true, enqueue SOURCE on TARGET.  Returns whether
   the message was delivered or whether SOURCE was enqueued on
   TARGET.  */
extern bool messenger_message_transfer (struct activity *activity,
					struct messenger *target,
					struct messenger *source,
					bool may_block);

/* If target is not blocked, load the message MESSAGE into TARGET.
   Returns whether the message was loaded.  NB: ANY CAPABILITY
   ADDRESSES ARE INTERPRETTED AS POINTERS TO STRUCT CAP!!! */
extern bool messenger_message_load (struct activity *activity,
				    struct messenger *target,
				    struct vg_message *message);

/* Attempt to deliver the message stored in TARGET to its thread.  If
   THREAD is activated, enqueues TARGET on it.  */
extern bool messenger_message_deliver (struct activity *activity,
				       struct messenger *target);

/* Unblock messenger MESSENGER.  If any messengers are waiting to
   deliver a message attempt delivery.  */
extern void messenger_unblock (struct activity *activity,
			       struct messenger *messenger);

/* Destroy the messenger MESSENGER: it is about to be deallocated.  */
extern void messenger_destroy (struct activity *activity,
			       struct messenger *messenger);

#endif
