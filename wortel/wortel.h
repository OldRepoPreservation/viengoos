/* wortel.h - RPC client stubs for wortel users.
   Copyright (C) 2004 Free Software Foundation, Inc.
   This file is part of the GNU Hurd.

   The GNU Hurd is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.

   The GNU Hurd is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with the GNU C Library; if not, write to the Free
   Software Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
   02111-1307 USA.  */

#ifndef _WORTEL_USER_H
#define _WORTEL_USER_H	1

#include <stdbool.h>

#include <l4/types.h>
#include <l4/space.h>
#include <l4/ipc.h>

#include <hurd/types.h>


/* The wortel capability ID is used to authenticate a message.  */
typedef l4_word_t wortel_cap_id_t;

/* The user must define these variables.  */
extern l4_thread_id_t wortel_thread_id;
extern wortel_cap_id_t wortel_cap_id;


/* Message labels.  */

/* A wortel message label consists of 16 bits.  The lower 8 bits
   specify the cap ID.  The next 7 bits specify the message ID.  The
   highest bit is always 0.  */
#define WORTEL_MSG_CAP_ID_BITS		8

#define WORTEL_MSG_THREAD_CONTROL	1
#define WORTEL_MSG_SPACE_CONTROL	2
#define WORTEL_MSG_PROCESSOR_CONTROL	3
#define WORTEL_MSG_MEMORY_CONTROL	4

#define WORTEL_MSG_PUTCHAR		32
#define WORTEL_MSG_SHUTDOWN		33

#define WORTEL_MSG_GET_MEM		64
#define WORTEL_MSG_GET_CAP_REQUEST	65
#define WORTEL_MSG_GET_CAP_REPLY	66
#define WORTEL_MSG_GET_THREADS		67
#define WORTEL_MSG_BOOTSTRAP_FINAL	68
#define WORTEL_MSG_GET_FIRST_FREE_THREAD_NO 69
#define WORTEL_MSG_GET_TASK_CAP_REQUEST	70
#define WORTEL_MSG_GET_TASK_CAP_REPLY	71
#define WORTEL_MSG_GET_DEVA_CAP_REQUEST	72
#define WORTEL_MSG_GET_DEVA_CAP_REPLY	73

#define _WORTEL_LABEL(id)					\
  (((id) << WORTEL_MSG_CAP_ID_BITS)				\
   | (wortel_cap_id & ((1 << WORTEL_MSG_CAP_ID_BITS) - 1)))


/* Privileged system calls.  */

/* Like l4_thread_control.  */
static inline l4_word_t
__attribute__((always_inline))
wortel_thread_control (l4_thread_id_t dest, l4_thread_id_t space,
		       l4_thread_id_t scheduler, l4_thread_id_t pager,
		       void *utcb)
{
  l4_msg_tag_t tag;
  l4_word_t result;

  l4_accept (L4_UNTYPED_WORDS_ACCEPTOR);

  tag = l4_niltag;
  l4_msg_tag_set_label (&tag, _WORTEL_LABEL (WORTEL_MSG_THREAD_CONTROL));
  l4_msg_tag_set_untyped_words (&tag, 5);
  l4_set_msg_tag (tag);
  l4_load_mr (1, dest);
  l4_load_mr (2, space);
  l4_load_mr (3, scheduler);
  l4_load_mr (4, pager);
  l4_load_mr (5, (l4_word_t) utcb);
  tag = l4_call (wortel_thread_id);

  l4_store_mr (1, &result);

  return result;
}


/* Like l4_space_control.  */
static inline l4_word_t
__attribute__((always_inline))
wortel_space_control (l4_thread_id_t space, l4_word_t control,
		      l4_fpage_t kip, l4_fpage_t utcb,
		      l4_thread_id_t redirector, l4_word_t *old_control)
{
  l4_msg_tag_t tag;
  l4_word_t result;

  l4_accept (L4_UNTYPED_WORDS_ACCEPTOR);

  tag = l4_niltag;
  l4_msg_tag_set_label (&tag, _WORTEL_LABEL (WORTEL_MSG_SPACE_CONTROL));
  l4_msg_tag_set_untyped_words (&tag, 5);
  l4_set_msg_tag (tag);
  l4_load_mr (1, space);
  l4_load_mr (2, control);
  l4_load_mr (3, kip);
  l4_load_mr (4, utcb);
  l4_load_mr (5, redirector);
  tag = l4_call (wortel_thread_id);

  l4_store_mr (1, &result);
  l4_store_mr (2, old_control);

  return result;
}


/* Like l4_processor_control.  */
static inline l4_word_t
__attribute__((always_inline))
wortel_processor_control (l4_word_t proc, l4_word_t control,
			  l4_word_t internal_freq, l4_word_t external_freq,
			  l4_word_t voltage)
{
  l4_msg_tag_t tag;
  l4_word_t result;

  l4_accept (L4_UNTYPED_WORDS_ACCEPTOR);

  tag = l4_niltag;
  l4_msg_tag_set_label (&tag, _WORTEL_LABEL (WORTEL_MSG_PROCESSOR_CONTROL));
  l4_msg_tag_set_untyped_words (&tag, 5);
  l4_set_msg_tag (tag);
  l4_load_mr (1, proc);
  l4_load_mr (2, control);
  l4_load_mr (3, internal_freq);
  l4_load_mr (4, external_freq);
  l4_load_mr (5, voltage);
  tag = l4_call (wortel_thread_id);

  l4_store_mr (1, &result);

  return result;
}


/* Like l4_set_pages_attributes, except that only up to 59 fpages can
   be provided.  */
static inline l4_word_t
__attribute__((always_inline))
wortel_memory_control (l4_word_t nr, l4_fpage_t *fpages, l4_word_t *attributes)
{
  l4_msg_tag_t tag;
  unsigned int i;
  l4_word_t result;

  l4_accept (L4_UNTYPED_WORDS_ACCEPTOR);

  tag = l4_niltag;
  l4_msg_tag_set_label (&tag, _WORTEL_LABEL (WORTEL_MSG_MEMORY_CONTROL));
  l4_msg_tag_set_untyped_words (&tag, nr + 4);
  l4_set_msg_tag (tag);
  for (i = 1; i <= nr; i++)
    l4_load_mr (i, fpages[i - 1]);
  l4_load_mr (i++, attributes[0]);
  l4_load_mr (i++, attributes[1]);
  l4_load_mr (i++, attributes[2]);
  l4_load_mr (i++, attributes[3]);
  tag = l4_call (wortel_thread_id);

  l4_store_mr (1, &result);

  return result;
}


/* Manager interface.  */

/* Echo the character CHR on the manager console.  */
static inline void
__attribute__((always_inline))
wortel_putchar (int chr)
{
  l4_msg_tag_t tag;

  l4_accept (L4_UNTYPED_WORDS_ACCEPTOR);

  tag = l4_niltag;
  l4_msg_tag_set_label (&tag, _WORTEL_LABEL (WORTEL_MSG_PUTCHAR));
  l4_msg_tag_set_untyped_words (&tag, 1);
  l4_set_msg_tag (tag);
  l4_load_mr (1, (l4_word_t) chr);
  tag = l4_call (wortel_thread_id);
}


/* Shutdown the system.  */
static inline void
__attribute__((always_inline))
wortel_shutdown (void)
{
  l4_msg_tag_t tag;

  l4_accept (L4_UNTYPED_WORDS_ACCEPTOR);

  tag = l4_niltag;
  l4_msg_tag_set_label (&tag, _WORTEL_LABEL (WORTEL_MSG_SHUTDOWN));
  l4_set_msg_tag (tag);
  tag = l4_call (wortel_thread_id);

  /* Never reached.  */
}


/* Boot interface.  */

/* Get some physical memory.  */
static inline l4_fpage_t
__attribute__((always_inline))
wortel_get_mem (void)
{
  l4_msg_tag_t tag;
  l4_word_t mrs[2];
  l4_grant_item_t grant_item;
  
  l4_accept (l4_map_grant_items (L4_COMPLETE_ADDRESS_SPACE));

  tag = l4_niltag;
  l4_msg_tag_set_label (&tag, _WORTEL_LABEL (WORTEL_MSG_GET_MEM));
  l4_set_msg_tag (tag);
  tag = l4_call (wortel_thread_id);

  l4_store_mr (1, &mrs[0]);
  l4_store_mr (2, &mrs[1]);
  grant_item = *((l4_grant_item_t *) mrs);

  return l4_grant_item_snd_fpage (grant_item);
}


/* Get the number of extra threads for physmem.  */
static inline l4_word_t
__attribute__((always_inline))
wortel_get_threads (void)
{
  l4_msg_tag_t tag;
  l4_word_t nr_threads;
  
  l4_accept (L4_UNTYPED_WORDS_ACCEPTOR);

  tag = l4_niltag;
  l4_msg_tag_set_label (&tag, _WORTEL_LABEL (WORTEL_MSG_GET_THREADS));
  l4_set_msg_tag (tag);
  tag = l4_call (wortel_thread_id);

  l4_store_mr (1, &nr_threads);
  return nr_threads;
}


/* Get the next physmem capability request.  */
static inline hurd_task_id_t
__attribute__((always_inline))
wortel_get_cap_request (unsigned int *nr_fpages, l4_fpage_t *fpages)
{
  l4_msg_tag_t tag;
  hurd_task_id_t task_id;
  unsigned int nr_items;
  unsigned int mr;
  l4_word_t mrs[2];

  l4_accept (l4_map_grant_items (L4_COMPLETE_ADDRESS_SPACE));

  tag = l4_niltag;
  l4_msg_tag_set_label (&tag, _WORTEL_LABEL (WORTEL_MSG_GET_CAP_REQUEST));
  l4_set_msg_tag (tag);
  tag = l4_call (wortel_thread_id);

  *nr_fpages = 0;
  mr = 1;
  l4_store_mr (mr++, &task_id);
  nr_items = l4_typed_words (tag) / 2;
  while (nr_items--)
    {
      /* FIX this code.  Do not break alias rule.  */
      l4_map_item_t *map_itemp = (l4_map_item_t *) mrs;

      l4_store_mrs (mr, 2, &mrs[0]);

      fpages[*nr_fpages] = l4_map_item_snd_fpage (*map_itemp);
      mr += 2;
      (*nr_fpages)++;
    }

  return task_id;
}


/* Reply to a capability request.  */
static inline void
__attribute__((always_inline))
wortel_get_cap_reply (hurd_cap_handle_t handle)
{
  l4_msg_tag_t tag;
  l4_word_t nr_threads;
  
  l4_accept (L4_UNTYPED_WORDS_ACCEPTOR);

  tag = l4_niltag;
  l4_msg_tag_set_label (&tag, _WORTEL_LABEL (WORTEL_MSG_GET_CAP_REPLY));
  l4_msg_tag_set_untyped_words (&tag, 1);
  l4_set_msg_tag (tag);
  l4_load_mr (1, handle);
  tag = l4_call (wortel_thread_id);
}


/* Get the task and deva cap handle.  */
static inline void
__attribute__((always_inline))
wortel_bootstrap_final (l4_thread_id_t *task_server,
			hurd_cap_handle_t *task_cap_handle,
			l4_thread_id_t *deva_server,
			hurd_cap_handle_t *deva_cap_handle)
{
  l4_msg_tag_t tag;
  
  l4_accept (L4_UNTYPED_WORDS_ACCEPTOR);

  tag = l4_niltag;
  l4_msg_tag_set_label (&tag, _WORTEL_LABEL (WORTEL_MSG_BOOTSTRAP_FINAL));
  l4_set_msg_tag (tag);
  tag = l4_call (wortel_thread_id);

  l4_store_mr (1, task_server);
  l4_store_mr (2, task_cap_handle);
  l4_store_mr (3, deva_server);
  l4_store_mr (4, deva_cap_handle);
}


/* Get the first free thread number for task.  */
static inline l4_word_t
__attribute__((always_inline))
wortel_get_first_free_thread_no (void)
{
  l4_msg_tag_t tag;
  l4_word_t first_free_thread_no;
  
  l4_accept (L4_UNTYPED_WORDS_ACCEPTOR);

  tag = l4_niltag;
  l4_msg_tag_set_label (&tag,
			_WORTEL_LABEL (WORTEL_MSG_GET_FIRST_FREE_THREAD_NO));
  l4_set_msg_tag (tag);
  tag = l4_call (wortel_thread_id);

  l4_store_mr (1, &first_free_thread_no);
  return first_free_thread_no;
}


/* Get the next task capability request.  */
static inline hurd_task_id_t
__attribute__((always_inline))
wortel_get_task_cap_request (unsigned int *nr_threads, l4_thread_id_t *threads)
{
  l4_msg_tag_t tag;
  hurd_task_id_t task_id;
  unsigned int nr_items;
  unsigned int mr;

  l4_accept (L4_UNTYPED_WORDS_ACCEPTOR);

  tag = l4_niltag;
  l4_msg_tag_set_label (&tag, _WORTEL_LABEL (WORTEL_MSG_GET_TASK_CAP_REQUEST));
  l4_set_msg_tag (tag);
  tag = l4_call (wortel_thread_id);

  *nr_threads = 0;
  mr = 1;
  l4_store_mr (mr++, &task_id);
  nr_items = l4_untyped_words (tag) - 1;
  while (nr_items--)
    l4_store_mr (mr++, &threads[(*nr_threads)++]);

  return task_id;
}


/* Reply to a task capability request.  */
static inline void
__attribute__((always_inline))
wortel_get_task_cap_reply (hurd_cap_handle_t handle)
{
  l4_msg_tag_t tag;
  l4_word_t nr_threads;
  
  l4_accept (L4_UNTYPED_WORDS_ACCEPTOR);

  tag = l4_niltag;
  l4_msg_tag_set_label (&tag, _WORTEL_LABEL (WORTEL_MSG_GET_TASK_CAP_REPLY));
  l4_msg_tag_set_untyped_words (&tag, 1);
  l4_set_msg_tag (tag);
  l4_load_mr (1, handle);
  tag = l4_call (wortel_thread_id);
}


/* Get the next task capability request.  */
static inline hurd_task_id_t
__attribute__((always_inline))
wortel_get_deva_cap_request (bool *master)
{
  l4_msg_tag_t tag;
  hurd_task_id_t task_id;
  l4_word_t is_master;

  l4_accept (L4_UNTYPED_WORDS_ACCEPTOR);

  tag = l4_niltag;
  l4_msg_tag_set_label (&tag, _WORTEL_LABEL (WORTEL_MSG_GET_DEVA_CAP_REQUEST));
  l4_set_msg_tag (tag);
  tag = l4_call (wortel_thread_id);

  l4_store_mr (1, &task_id);
  l4_store_mr (2, &is_master);

  *master = is_master ? true : false;

  return task_id;
}


/* Reply to a task capability request.  */
static inline void
__attribute__((always_inline))
wortel_get_deva_cap_reply (hurd_cap_handle_t handle)
{
  l4_msg_tag_t tag;
  l4_word_t nr_threads;
  
  l4_accept (L4_UNTYPED_WORDS_ACCEPTOR);

  tag = l4_niltag;
  l4_msg_tag_set_label (&tag, _WORTEL_LABEL (WORTEL_MSG_GET_DEVA_CAP_REPLY));
  l4_msg_tag_set_untyped_words (&tag, 1);
  l4_set_msg_tag (tag);
  l4_load_mr (1, handle);
  tag = l4_call (wortel_thread_id);
}


#endif	/* _WORTEL_USER_H */
