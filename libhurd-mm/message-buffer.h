/* message-buffer.h - Interface for managing messaging data structures.
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

#ifndef __have_hurd_message_buffer
# define __have_hurd_message_buffer

# include <stdint.h>
# include <stdbool.h>
# include <viengoos/addr.h>

/* Forward.  */
struct vg_message;

#define HURD_MESSAGE_BUFFER_MAGIC 0x111A61C

struct hurd_message_buffer
{
  uintptr_t magic;

  struct hurd_message_buffer *next;

  /* A messenger associated REQUEST.  The messenger's identifier is
     set to the data structure's address.  */
  vg_addr_t sender;
  struct vg_message *request;
  /* A messenger associated with REPLY.  The messenger's identifier is
     set to the data structure's address.  */
  vg_addr_t receiver_strong;
  /* A weakened version.  */
  vg_addr_t receiver;
  struct vg_message *reply;

  /* If not NULL, then this routine is called.  */
  void (*closure) (struct hurd_message_buffer *mb);

  /* XXX: Whether the activation should resume the thread or simply
     free the buffer.  Ignored if callback is not NULL.  */
  bool just_free;

  void *cookie;
};

#endif  /* __have_hurd_message_buffer */

#ifdef __need_hurd_message_buffer
# undef __need_hurd_message_buffer
#else

# ifndef _HURD_MESSAGE_BUFFER
# define _HURD_MESSAGE_BUFFER

/* Allocate a message buffer.  */
extern struct hurd_message_buffer *hurd_message_buffer_alloc (void);

/* Allocate a message buffer, which is unlikely to be freed soon.  */
extern struct hurd_message_buffer *hurd_message_buffer_alloc_long (void);

/* Free a message buffer.  */
extern void hurd_message_buffer_free (struct hurd_message_buffer *buf);

# endif /* _HURD_MESSAGE_BUFFER */

#endif /* !__need_hurd_message_buffer */
