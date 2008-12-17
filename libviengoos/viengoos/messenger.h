/* messenger.h - Messenger buffer definitions.
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

#ifndef _VIENGOOS_MESSENGER_H
#define _VIENGOOS_MESSENGER_H 1

#include <stdint.h>
#include <viengoos/addr.h>

/* A messenger references a message buffer.  It can transfer a message
   (contained in its message buffer) to another messenger.  It can
   also receive a message from another messenger.  A messenger can
   block waiting to deliver a message to or receive a message from
   another messenger.

   To send a message, a payload is loaded into a message buffer and
   associated with a messenger.  The messenger is then enqueued on
   another messenger.  When the latter messenger is unblocked, the
   message is delivered.

   To avoid messages from being overwritten, messengers are blocked on
   message delivery and must be explicitly unblocked before another
   message is sent.  */
#ifdef RM_INTERN
struct messenger;
typedef struct messenger *vg_messenger_t;
#else
typedef vg_addr_t vg_messenger_t;
#endif

#define VG_MESSENGER_INLINE_WORDS 2
#define VG_MESSENGER_INLINE_CAPS 1

/* Number of user-settable capability slots at the start of the
   messenger structure.  */
enum
  {
    /* The thread to activate.  */
    VG_MESSENGER_THREAD_SLOT = 0,
    /* The address space root relative to which all capability
       addresses in the message buffer will be resolved.  */
    VG_MESSENGER_ASROOT_SLOT,
    /* The assocaited message buffer.  */
    VG_MESSENGER_BUFFER_SLOT,
    /* The activity that was delivered with the last message.  */
    VG_MESSENGER_ACTIVITY_SLOT,
    
    VG_MESSENGER_SLOTS = 4,
  };
#define VG_MESSENGER_SLOTS_LOG2 2

enum
  {
    VG_messenger_id = 900,
  };

#define RPC_STUB_PREFIX vg
#define RPC_ID_PREFIX VG

#include <viengoos/rpc.h>

/* Set MESSENGER's ID to ID and return the old ID in OLD.  */
RPC(messenger_id, 1, 1, 0,
    /* cap_t activity, cap_t messenger, */
    uint64_t, id, uint64_t, old)

#undef RPC_STUB_PREFIX vg
#undef RPC_ID_PREFIX VG

#endif /* _VIENGOOS_MESSENGER_H  */
