/* ipc.h - Interprocess communication interface.
   Copyright (C) 2008 Free Software Foundation, Inc.
   Written by Neal H. Walfield <neal@gnu.org>.

   This file is part of the GNU Hurd.

   GNU Hurd is free software: you can redistribute it and/or modify it
   under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   GNU Hurd is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with GNU Hurd.  If not, see
   <http://www.gnu.org/licenses/>.  */

#ifndef _VIENGOOS_IPC_H
#define _VIENGOOS_IPC_H 1

#include <stdint.h>
#include <errno.h>
#include <hurd/addr.h>
#include <hurd/stddef.h>
#include <hurd/message.h>
#include <assert.h>

#ifdef USE_L4
#include <l4.h>
#endif

/* IPC flags.  */
enum
  {
    /* IPC includes a receive phase.  */
    VG_IPC_RECEIVE = 1 << 0,
    /* Don't unblock the receive buffer if there is no message queued
       for delivery.  */
    VG_IPC_RECEIVE_NONBLOCKING = 1 << 1,
    /* Activate the thread on message receipt.  */
    VG_IPC_RECEIVE_ACTIVATE = 1 << 2,
    /* Set the receive messenger's thread to the caller.  */
    VG_IPC_RECEIVE_SET_THREAD_TO_CALLER = 1 << 3,
    /* Set the receive messener's address space root to the
       caller's.  */
    VG_IPC_RECEIVE_SET_ASROOT_TO_CALLERS = 1 << 4,
    /* Whether to receive the message inline.  */
    VG_IPC_RECEIVE_INLINE = 1 << 5,
    /* Whether to receive any capabilities inline when receiving a
       message inline (i.e., when VG_IPC_RECEIVE_INLINE is set).  */
    VG_IPC_RECEIVE_INLINE_CAP1 = 1 << 6,

    /* IPC includes a send phase.  */
    VG_IPC_SEND = 1 << 7,
    /* If the object is blocked, return EWOULDBLOCK.  */
    VG_IPC_SEND_NONBLOCKING = 1 << 8,
    /* Activate the thread on message transfer.  */
    VG_IPC_SEND_ACTIVATE = 1 << 9,
    /* Set the send messenger's thread to the caller.  */
    VG_IPC_SEND_SET_THREAD_TO_CALLER = 1 << 10,
    /* Set the sender messener's address space root to the
       caller's.  */
    VG_IPC_SEND_SET_ASROOT_TO_CALLERS = 1 << 11,
    /* Whether to send the message inline.  */
    VG_IPC_SEND_INLINE = 1 << 12,

    /* Which inline data to transfer when sending a message.  Inline
       data is ignored if the send buffer is not ADDR_VOID.  */
    VG_IPC_SEND_INLINE_WORD1 = 1 << 13,
    VG_IPC_SEND_INLINE_WORD2 = 1 << 14,
    VG_IPC_SEND_INLINE_CAP1 = 1 << 15,


    /* The IPC includes a return phase.  */
    VG_IPC_RETURN = 1 << 16,

  };

#ifndef RM_INTERN
/* An IPC consists of three phases: the receive phase, the send phase
   and the return phase.  All three phases are optional.  Each phase
   is executed after the previous phase has completed.  If a phase
   does not complete successfully, the phase is aborted and the
   remaining phases are not executed.


   RECEIVE PHASE

   If FLAGS contains VG_IPC_RECEIVE, the IPC includes a receive phase.

   If RECV_BUF is not ADDR_VOID, associates RECV_BUF with
   RECV_MESSENGER.

   If FLAGS contains VG_IPC_RECEIVE_NONBLOCKING:

     Unblocks RECV_MESSENGER if RECV_MESSENGER has a messenger waiting
     to deliver a message.  Otherwise, returns EWOUDBLOCK.

   Otherwise:

     Unblocks RECV_MESSENGER.

   Resources are charged to RECV_ACTIVITY.

   If VG_IPC_RECEIVE_ACTIVATE is set, an activation is sent to the
   thread associated with RECV_MESSENGER when RECV_MESSENGER receives
   a message.


   SEND PHASE

   If FLAGS contains VG_IPC_SEND, the IPC includes a send phase.

   If SEND_MESSENGER is ADDR_VOID, an implicit messenger is allocated
   and VG_IPC_SEND_NONBLOCKING is assumed to be on.

   If SEND_BUF is not ADDR_VOID, assocaiates SEND_BUF with
   SEND_MESSENGER.  Otherwise, associates inline data (INLINE_WORD1,
   INLINE_WORD2 and INLINE_CAP) according to the inline flags with
   SEND_MESSENGER.

   If FLAGS contains VG_IPC_SEND_NONBLOCKING:

     If TARGET_MESSENGER is blocked, returns ETIMEDOUT.

   Otherwise:

     Blocks SEND_MESSENGER and enqueues it on TARGET_MESSENGER.

   When TARGET_MESSENGER becomes unblocked, SEND_MESSENGER delivers
   its message to TARGET_MESSENGER.

   Resources are charged to SEND_ACTIVITY.

   If VG_IPC_SEND_ACTIVATE is set, an activation is sent to the thread
   associated with SEND_MESSENGER when SEND_MESSENGER's message is
   transferred to TARGET_MESSENGER (or, when TARGET_MESSENGER is
   destroyed).


   RETURN PHASE

   If FLAGS contains VG_IPC_RETURN, the IPC returns.  Otherwise, the
   calling thread is suspended until it is next activated.  */
static inline error_t
vg_ipc_full (uintptr_t flags,
	     addr_t recv_activity, addr_t recv_messenger, addr_t recv_buf,
	     addr_t recv_inline_cap,
	     addr_t send_activity, addr_t target_messenger,
	     addr_t send_messenger, addr_t send_buf,
	     uintptr_t send_inline_word1, uintptr_t send_inline_word2,
	     addr_t send_inline_cap)
{
  error_t err = 0;

#ifdef USE_L4
  l4_msg_tag_t tag = l4_niltag;
  l4_msg_tag_set_label (&tag, 8194);

  l4_msg_t msg;
  l4_msg_clear (msg);
  l4_msg_set_msg_tag (msg, tag);

  void msg_append_addr (addr_t addr)
  {
    int i;
    for (i = 0; i < sizeof (addr_t) / sizeof (uintptr_t); i ++)
      l4_msg_append_word (msg, ((uintptr_t *) &addr)[i]);
  }

  l4_msg_append_word (msg, flags);

  msg_append_addr (recv_activity);
  msg_append_addr (recv_messenger);
  msg_append_addr (recv_buf);
  msg_append_addr (recv_inline_cap);

  msg_append_addr (send_activity);
  msg_append_addr (target_messenger);

  msg_append_addr (send_messenger);
  msg_append_addr (send_buf);

  l4_msg_append_word (msg, send_inline_word1);
  l4_msg_append_word (msg, send_inline_word2);
  msg_append_addr (send_inline_cap);

  l4_msg_load (msg);
  l4_accept (l4_map_grant_items (L4_COMPLETE_ADDRESS_SPACE));

  bool call = true;

  while (1)
    {
      extern struct hurd_startup_data *__hurd_startup_data;      

      if (call)
	tag = l4_call (__hurd_startup_data->rm);
      else
	tag = l4_receive (__hurd_startup_data->rm);

      if (likely (l4_ipc_failed (tag)))
	{
	  if (((l4_error_code () >> 1) & 0x7) == 3)
	    {
	      if (l4_error_code () & 1)
		/* IPC was interrupted in the receive phase, i.e., we
		   got a response.  */
		break;
	      else
		call = false;
	    }
	  else
	    return EHOSTDOWN;
	}
      else
	{
	  assert (l4_untyped_words (tag) == 1);
	  l4_msg_store (tag, msg);
	  /* Potential error performing IPC (or VG_RETURN specified).  */
	  err = l4_msg_word (msg, 1);
	  break;
	}
    }
#else
# warning vg_ipc not ported to this architecture.
#endif

  return err;
}

static inline error_t
vg_ipc (uintptr_t flags,
	addr_t recv_activity, addr_t recv_messenger, addr_t recv_buf,
	addr_t send_activity, addr_t target_messenger,
	addr_t send_messenger, addr_t send_buf)
{
  return vg_ipc_full (flags,
		      recv_activity, recv_messenger, recv_buf, ADDR_VOID,
		      send_activity, target_messenger,
		      send_messenger, send_buf,
		      0, 0, ADDR_VOID);
}

static inline error_t
vg_ipc_short (uintptr_t flags,
	      addr_t recv_activity, addr_t recv_messenger, addr_t recv_cap,
	      addr_t send_activity, addr_t target_messenger,
	      addr_t send_messenger,
	      uintptr_t inline_word1, uintptr_t inline_word2,
	      addr_t inline_cap)
{
  return vg_ipc_full (flags, 
		      recv_activity, recv_messenger, ADDR_VOID, recv_cap,
		      send_activity, target_messenger,
		      send_messenger, ADDR_VOID,
		      inline_word1, inline_word2, inline_cap);
}

static inline error_t
vg_send (uintptr_t flags, addr_t send_activity, addr_t target_messenger,
	 addr_t send_messenger, addr_t send_buf)
{
  return vg_ipc_full (flags | VG_IPC_SEND | VG_IPC_SEND_ACTIVATE,
		      ADDR_VOID, ADDR_VOID, ADDR_VOID, ADDR_VOID,
		      send_activity, target_messenger,
		      send_messenger, send_buf,
		      0, 0, ADDR_VOID);
}

static inline error_t
vg_reply (uintptr_t flags, addr_t send_activity, addr_t target_messenger,
	  addr_t send_messenger, addr_t send_buf)
{
  return vg_ipc_full (flags | VG_IPC_SEND | VG_IPC_SEND_NONBLOCKING,
		      ADDR_VOID, ADDR_VOID, ADDR_VOID, ADDR_VOID,
		      send_activity, target_messenger, send_messenger, send_buf,
		      0, 0, ADDR_VOID);
}

/* Suspend the caller until the next activation.  */
static inline error_t
vg_suspend (void)
{
  return vg_ipc_full (0,
		      ADDR_VOID, ADDR_VOID, ADDR_VOID, ADDR_VOID,
		      ADDR_VOID, ADDR_VOID, ADDR_VOID, ADDR_VOID,
		      0, 0, ADDR_VOID);
}

#endif

#endif
