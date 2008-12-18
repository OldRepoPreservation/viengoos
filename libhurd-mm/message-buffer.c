/* message-buffer.c - Implementation of messaging data structure management.
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

#include <hurd/stddef.h>
#include <hurd/slab.h>
#include <hurd/storage.h>
#include <hurd/as.h>
#include <hurd/startup.h>
#include <hurd/capalloc.h>
#include <hurd/mm.h>

extern struct hurd_startup_data *__hurd_startup_data;      

static char initial_pages[4][PAGESIZE] __attribute__ ((aligned (PAGESIZE)));
static int initial_page;
#define INITIAL_PAGE_COUNT (sizeof (initial_pages) / sizeof (initial_pages[0]))
static int initial_messenger;
#define INITIAL_MESSENGER_COUNT				\
  (sizeof (__hurd_startup_data->messengers)		\
   / sizeof (__hurd_startup_data->messengers[0]))

static error_t
slab_alloc (void *hook, size_t size, void **ptr)
{
  assert (size == PAGESIZE);

  if (unlikely (initial_page < INITIAL_PAGE_COUNT))
    {
      *ptr = initial_pages[initial_page ++];
      return 0;
    }

  struct storage storage = storage_alloc (meta_data_activity, vg_cap_page,
					  STORAGE_LONG_LIVED,
					  VG_OBJECT_POLICY_DEFAULT,
					  VG_ADDR_VOID);
  if (VG_ADDR_IS_VOID (storage.addr))
    panic ("Out of space.");
  *ptr = VG_ADDR_TO_PTR (vg_addr_extend (storage.addr, 0, PAGESIZE_LOG2));

  return 0;
}

static error_t
slab_dealloc (void *hook, void *buffer, size_t size)
{
  assert (size == PAGESIZE);

  vg_addr_t addr = vg_addr_chop (VG_PTR_TO_ADDR (buffer), PAGESIZE_LOG2);
  storage_free (addr, false);

  return 0;
}

static error_t
slab_constructor (void *hook, void *object)
{
  struct hurd_message_buffer *mb = object;
  assert (mb->magic == 0);
  mb->magic = ~HURD_MESSAGE_BUFFER_MAGIC;

  return 0;
}

static void
slab_destructor (void *hook, void *object)
{
  struct hurd_message_buffer *mb = object;

  if (mb->magic != HURD_MESSAGE_BUFFER_MAGIC)
    /* It was never initialized.  */
    {
      assert (mb->magic == ~HURD_MESSAGE_BUFFER_MAGIC);
      return;
    }

  storage_free (mb->sender, false);
  storage_free (vg_addr_chop (VG_PTR_TO_ADDR (mb->request), PAGESIZE_LOG2),
		false);
  storage_free (mb->receiver, false);
  storage_free (vg_addr_chop (VG_PTR_TO_ADDR (mb->reply), PAGESIZE_LOG2),
		false);
}

/* Storage descriptors are alloced from a slab.  */
static struct hurd_slab_space message_buffer_slab
  = HURD_SLAB_SPACE_INITIALIZER (struct hurd_message_buffer,
				 slab_alloc, slab_dealloc,
				 slab_constructor, slab_destructor, NULL);


static struct hurd_message_buffer *
hurd_message_buffer_alloc_hard (void)
{
  void *buffer;
  error_t err = hurd_slab_alloc (&message_buffer_slab, &buffer);
  if (err)
    panic ("Out of memory!");

  struct hurd_message_buffer *mb = buffer;

  if (mb->magic == HURD_MESSAGE_BUFFER_MAGIC)
    /* It's already initialized.  */
    return mb;

  assert (mb->magic == ~HURD_MESSAGE_BUFFER_MAGIC);
  mb->magic = HURD_MESSAGE_BUFFER_MAGIC;

  struct storage storage;

  /* The send messenger.  */
  if (unlikely (initial_messenger < INITIAL_MESSENGER_COUNT))
    mb->sender = __hurd_startup_data->messengers[initial_messenger ++];
  else
    {
      storage = storage_alloc (meta_data_activity, vg_cap_messenger,
			       STORAGE_LONG_LIVED,
			       VG_OBJECT_POLICY_DEFAULT, VG_ADDR_VOID);
      if (VG_ADDR_IS_VOID (storage.addr))
	panic ("Out of space.");

      mb->sender = storage.addr;
    }

  /* The receive messenger.  */
  if (unlikely (initial_messenger < INITIAL_MESSENGER_COUNT))
    mb->receiver_strong = __hurd_startup_data->messengers[initial_messenger ++];
  else
    {
      storage = storage_alloc (meta_data_activity, vg_cap_messenger,
			       STORAGE_LONG_LIVED,
			       VG_OBJECT_POLICY_DEFAULT, VG_ADDR_VOID);
      if (VG_ADDR_IS_VOID (storage.addr))
	panic ("Out of space.");

      mb->receiver_strong = storage.addr;
    }

  /* Weaken it.  */
#if 0
  mb->receiver = capalloc ();
  struct vg_cap receiver_cap = as_cap_lookup (mb->receiver_strong,
					      vg_cap_messenger, NULL);
  assert (receiver_cap.type == vg_cap_messenger);
  as_slot_lookup_use
    (mb->receiver,
     ({
       bool ret = vg_cap_copy_x (VG_ADDR_VOID,
			      VG_ADDR_VOID, slot, mb->receiver,
			      VG_ADDR_VOID, receiver_cap, mb->receiver_strong,
			      VG_CAP_COPY_WEAKEN,
			      VG_CAP_PROPERTIES_VOID);
       assert (ret);
     }));
#endif
  mb->receiver = mb->receiver_strong;

  /* The send buffer.  */
  if (unlikely (initial_page < INITIAL_PAGE_COUNT))
    mb->request = (void *) &initial_pages[initial_page ++][0];
  else
    {
      storage = storage_alloc (meta_data_activity, vg_cap_page,
			       STORAGE_LONG_LIVED,
			       VG_OBJECT_POLICY_DEFAULT, VG_ADDR_VOID);
      if (VG_ADDR_IS_VOID (storage.addr))
	panic ("Out of space.");

      mb->request = VG_ADDR_TO_PTR (vg_addr_extend (storage.addr,
						    0, PAGESIZE_LOG2));
    }

  /* And the receive buffer.  */
  if (unlikely (initial_page < INITIAL_PAGE_COUNT))
    mb->reply = (void *) &initial_pages[initial_page ++][0];
  else
    {
      storage = storage_alloc (meta_data_activity, vg_cap_page,
			       STORAGE_LONG_LIVED,
			       VG_OBJECT_POLICY_DEFAULT, VG_ADDR_VOID);
      if (VG_ADDR_IS_VOID (storage.addr))
	panic ("Out of space.");

      mb->reply = VG_ADDR_TO_PTR (vg_addr_extend (storage.addr,
						  0, PAGESIZE_LOG2));
    }


  /* Now set the messengers' id.  */
  vg_messenger_id_receive_marshal (mb->reply);
  vg_messenger_id_send_marshal (mb->request,
				(uint64_t) (uintptr_t) mb,
				mb->receiver);

  /* Set the reply messenger's id first as the activation handler
     requires that it be set correctly.  This will do that just before
     the reply is sent.  */
  hurd_activation_message_register (mb);
  err = vg_ipc_full (VG_IPC_RECEIVE | VG_IPC_SEND | VG_IPC_RECEIVE_ACTIVATE
		     | VG_IPC_RECEIVE_SET_THREAD_TO_CALLER
		     | VG_IPC_SEND_SET_THREAD_TO_CALLER,
		     VG_ADDR_VOID, mb->receiver, VG_PTR_TO_PAGE (mb->reply),
		     VG_ADDR_VOID,
		     VG_ADDR_VOID, mb->receiver,
		     mb->sender, VG_PTR_TO_PAGE (mb->request),
		     0, 0, VG_ADDR_VOID);
  if (err)
    panic ("Failed to set receiver's id");

  err = vg_messenger_id_reply_unmarshal (mb->reply, NULL);
  if (err)
    panic ("Setting receiver's id: %d", err);

  hurd_activation_message_register (mb);
  err = vg_ipc_full (VG_IPC_RECEIVE | VG_IPC_SEND | VG_IPC_RECEIVE_ACTIVATE,
		     VG_ADDR_VOID, mb->receiver, VG_PTR_TO_PAGE (mb->reply),
		     VG_ADDR_VOID,
		     VG_ADDR_VOID, mb->sender,
		     mb->sender, VG_PTR_TO_PAGE (mb->request),
		     0, 0, VG_ADDR_VOID);
  if (err)
    panic ("Failed to set sender's id");
  
  err = vg_messenger_id_reply_unmarshal (mb->reply, NULL);
  if (err)
    panic ("Setting sender's id: %d", err);

  return mb;
}

static struct hurd_message_buffer *buffers;
static int buffers_count;

void
hurd_message_buffer_free (struct hurd_message_buffer *buffer)
{
  /* XXX We should perhaps free some buffers if we go over a high
     water mark.  */
  // hurd_slab_dealloc (&message_buffer_slab, buffer);

  /* Add BUFFER to the free list.  */
  for (;;)
    {
      buffer->next = buffers;
      if (__sync_val_compare_and_swap (&buffers, buffer->next, buffer)
	  == buffer->next)
	{
	  __sync_fetch_and_add (&buffers_count, 1);
	  return;
	}
    }
}

static int
num_threads (void)
{
  extern int __pthread_num_threads __attribute__ ((weak));

  if (&__pthread_num_threads)
    return __pthread_num_threads;
  else
    return 1;
}

#define BUFFERS_LOW_WATER (4 + num_threads () * 2)
#define BUFFERS_HIGH_WATER (8 + num_threads () * 3)

struct hurd_message_buffer *
hurd_message_buffer_alloc (void)
{
  struct hurd_message_buffer *mb;
  do
    {
      static int allocating;

      if (likely (mm_init_done)
	  && unlikely (buffers_count <= BUFFERS_LOW_WATER)
	  && ! allocating
	  && __sync_val_compare_and_swap (&allocating, 0, 1) == 0)
	{
	  for (;;)
	    {
	      mb = hurd_message_buffer_alloc_hard ();

	      if (buffers_count == BUFFERS_HIGH_WATER)
		break;

	      hurd_message_buffer_free (mb);
	    }

	  allocating = 0;
	  return mb;
	}

      mb = buffers;
      if (! mb)
	{
	  mb = hurd_message_buffer_alloc_hard ();
	  return mb;
	}
    }
  while (__sync_val_compare_and_swap (&buffers, mb, mb->next) != mb);
  __sync_fetch_and_add (&buffers_count, -1);

  return mb;
}

struct hurd_message_buffer *
hurd_message_buffer_alloc_long (void)
{
  return hurd_message_buffer_alloc_hard ();
}
