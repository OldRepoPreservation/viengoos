/* l4/gnu/ipc.h - Public GNU interface for L4 IPC.
   Copyright (C) 2004 Free Software Foundation, Inc.
   Written by Marcus Brinkmann <marcus@gnu.org>.

   This file is part of the GNU L4 library.
 
   The GNU L4 library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public License
   as published by the Free Software Foundation; either version 2.1 of
   the License, or (at your option) any later version.
 
   The GNU L4 library is distributed in the hope that it will be
   useful, but WITHOUT ANY WARRANTY; without even the implied warranty
   of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU Lesser General Public License for more details.
 
   You should have received a copy of the GNU Lesser General Public
   License along with the GNU L4 library; if not, write to the Free
   Software Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
   02111-1307, USA.  */

#ifndef _L4_IPC_H
# error "Never use <l4/gnu/ipc.h> directly; include <l4/ipc.h> instead."
#endif


typedef _L4_msg_tag_t l4_msg_tag_t;

#define	l4_niltag	_L4_niltag

static inline l4_word_t
_L4_attribute_always_inline
l4_is_msg_tag_equal (l4_msg_tag_t l, l4_msg_tag_t r)
{
  return _L4_is_msg_tag_equal (l, r);
}


static inline l4_word_t
_L4_attribute_always_inline
l4_is_msg_tag_not_equal (l4_msg_tag_t l, l4_msg_tag_t r)
{
  return _L4_is_msg_tag_not_equal (l, r);
}


static inline l4_word_t
_L4_attribute_always_inline
l4_label (l4_msg_tag_t t)
{
  return _L4_label (t);
}


static inline l4_word_t
_L4_attribute_always_inline
l4_untyped_words (l4_msg_tag_t t)
{
  return _L4_untyped_words (t);
}


static inline l4_word_t
_L4_attribute_always_inline
l4_typed_words (l4_msg_tag_t t)
{
  return _L4_typed_words (t);
}


static inline l4_msg_tag_t
_L4_attribute_always_inline
l4_msg_tag_add_label (l4_msg_tag_t tag, l4_word_t label)
{
  return _L4_msg_tag_add_label (tag, label);
}


static inline void
_L4_attribute_always_inline
l4_msg_tag_add_label_to (l4_msg_tag_t *tag, l4_word_t label)
{
  return _L4_msg_tag_add_label_to (tag, label);
}


static inline l4_msg_tag_t
_L4_attribute_always_inline
l4_msg_tag (void)
{
  return _L4_msg_tag ();
}


static inline void
_L4_attribute_always_inline
l4_set_msg_tag (l4_msg_tag_t tag)
{
  _L4_set_msg_tag (tag);
}



/* Map items.  */

typedef _L4_map_item_t l4_map_item_t;

static inline l4_map_item_t
_L4_attribute_always_inline
l4_map_item (l4_fpage_t fpage, l4_word_t send_base)
{
  return _L4_map_item (fpage, send_base);
}


static inline l4_word_t
_L4_attribute_always_inline
l4_is_map_item (l4_map_item_t map_item)
{
  return _L4_is_map_item (map_item);
}


static inline l4_word_t
_L4_attribute_always_inline
l4_map_item_snd_fpage (l4_map_item_t map_item)
{
  return _L4_map_item_snd_fpage (map_item);
}


static inline l4_word_t
_L4_attribute_always_inline
l4_map_item_snd_base (l4_map_item_t map_item)
{
  return _L4_map_item_snd_base (map_item);
}


/* Grant items.  */

typedef _L4_grant_item_t l4_grant_item_t;

static inline l4_grant_item_t
_L4_attribute_always_inline
l4_grant_item (l4_fpage_t fpage, l4_word_t send_base)
{
  return _L4_grant_item (fpage, send_base);
}


static inline l4_word_t
_L4_attribute_always_inline
l4_is_grant_item (l4_grant_item_t grant_item)
{
  return _L4_is_grant_item (grant_item);
}


static inline l4_word_t
_L4_attribute_always_inline
l4_grant_item_snd_fpage (l4_grant_item_t grant_item)
{
  return _L4_grant_item_snd_fpage (grant_item);
}


static inline l4_word_t
_L4_attribute_always_inline
l4_grant_item_snd_base (l4_grant_item_t grant_item)
{
  return _L4_grant_item_snd_base (grant_item);
}


/* String items.  */

typedef _L4_string_item_t l4_string_item_t;

typedef _L4_cache_allocation_hint_t l4_cache_allocation_hint_t;

#define L4_use_default_cache_line_allocation \
  _L4_use_default_cache_line_allocation

static inline l4_word_t
_L4_attribute_always_inline
l4_is_string_item (l4_string_item_t *string_item)
{
  return _L4_is_string_item (string_item);
}


static inline l4_word_t
_L4_attribute_always_inline
l4_compound_string (l4_string_item_t *string_item)
{
  return _L4_compound_string (string_item);
}


static inline l4_word_t
_L4_attribute_always_inline
l4_substrings (l4_string_item_t *string_item)
{
  return _L4_substrings (string_item);
}


static inline void *
_L4_attribute_always_inline
l4_substring (l4_string_item_t *string_item, l4_word_t nr)
{
  return _L4_substring (string_item, nr);
}


static inline l4_string_item_t *
_L4_attribute_always_inline
l4_add_substring_address_to (l4_string_item_t *string_item,
			     l4_string_item_t *source)
{
  return _L4_add_substring_address_to (string_item, source);
}


static inline l4_string_item_t *
_L4_attribute_always_inline
l4_add_substring_to (l4_string_item_t *string_item, void *source)
{
  return _L4_add_substring_to (string_item, source);
}


static inline
_L4_attribute_always_inline
l4_cache_allocation_hint_t
l4_cache_allocation_hint (l4_string_item_t string_item)
{
  return _L4_cache_allocation_hint (string_item);
}


static inline
_L4_attribute_always_inline
l4_string_item_t
l4_add_cache_allocation_hint (l4_string_item_t string_item,
			      l4_cache_allocation_hint_t hint)
{
  return _L4_add_cache_allocation_hint (string_item, hint);
}


static inline l4_string_item_t *
_L4_attribute_always_inline
l4_add_cache_allocation_hint_to (l4_string_item_t *string_item,
				 l4_cache_allocation_hint_t hint)
{
  return _L4_add_cache_allocation_hint_to (string_item, hint);
}



/* Acceptors and message buffers.  */
typedef _L4_acceptor_t l4_acceptor_t;


#define L4_UNTYPED_WORDS_ACCEPTOR	_L4_untyped_words_acceptor
#define L4_STRING_ITEMS_ACCEPTOR	_L4_string_items_acceptor

typedef _L4_msg_buffer_t l4_msg_buffer_t;

static inline l4_acceptor_t
_L4_attribute_always_inline
l4_map_grant_items (l4_fpage_t rcv_window)
{
  return _L4_map_grant_items (rcv_window);
}


static inline l4_acceptor_t
_L4_attribute_always_inline
l4_add_acceptor (l4_acceptor_t acceptor1, l4_acceptor_t acceptor2)
{
  return _L4_add_acceptor (acceptor1, acceptor2);
}


static inline l4_acceptor_t *
_L4_attribute_always_inline
l4_add_acceptor_to (l4_acceptor_t *acceptor1, l4_acceptor_t acceptor2)
{
  return _L4_add_acceptor_to (acceptor1, acceptor2);
}


static inline l4_acceptor_t
_L4_attribute_always_inline
l4_remove_acceptor (l4_acceptor_t acceptor1, l4_acceptor_t acceptor2)
{
  return _L4_remove_acceptor (acceptor1, acceptor2);
}


static inline l4_acceptor_t *
_L4_attribute_always_inline
l4_remove_acceptor_from (l4_acceptor_t *acceptor1, l4_acceptor_t acceptor2)
{
  return _L4_remove_acceptor_from (acceptor1, acceptor2);
}


static inline l4_word_t
_L4_attribute_always_inline
l4_has_string_items (l4_acceptor_t acceptor)
{
  return _L4_has_string_items (acceptor);
}


static inline l4_word_t
_L4_attribute_always_inline
l4_has_map_grant_items (l4_acceptor_t acceptor)
{
  return _L4_has_map_grant_items (acceptor);
}


static inline l4_fpage_t
_L4_attribute_always_inline
l4_rcv_window (l4_acceptor_t acceptor)
{
  return _L4_rcv_window (acceptor);
}


static inline void
_L4_attribute_always_inline
l4_accept (l4_acceptor_t acceptor)
{
  _L4_accept (acceptor);
}


static inline void
_L4_attribute_always_inline
l4_accept_strings (l4_acceptor_t acceptor, l4_msg_buffer_t msg_buffer)
{
  _L4_accept_strings (acceptor, msg_buffer);
}


static inline l4_acceptor_t
_L4_attribute_always_inline
l4_accepted (void)
{
  return _L4_accepted ();
}


static inline void
_L4_attribute_always_inline
l4_msg_buffer_clear (l4_msg_buffer_t msg_buffer)
{
  _L4_msg_buffer_clear (msg_buffer);
}


static inline void
_L4_attribute_always_inline
l4_msg_buffer_append_simple_rcv_string (l4_msg_buffer_t msg_buffer,
					l4_string_item_t string_item)
{
  return _L4_msg_buffer_append_simple_rcv_string (msg_buffer, string_item);
}


static inline void
_L4_attribute_always_inline
l4_msg_buffer_append_rcv_string (l4_msg_buffer_t msg_buffer,
				 l4_string_item_t *string_item)
{
  _L4_msg_buffer_append_rcv_string (msg_buffer, string_item);
}



/* Message composition.  */

typedef _L4_msg_t l4_msg_t;

static inline void
_L4_attribute_always_inline
l4_msg_put (l4_msg_t msg, l4_word_t label, int untyped_nr,
			       l4_word_t *untyped, int typed_nr, void *typed)
{
  _L4_msg_put (msg, label, untyped_nr, untyped, typed_nr, typed);
}


static inline void
_L4_attribute_always_inline
l4_msg_get (l4_msg_t msg, l4_word_t *untyped, void *typed)
{
  _L4_msg_get (msg, untyped, typed);
}


static inline l4_msg_tag_t
_L4_attribute_always_inline
l4_msg_msg_tag (l4_msg_t msg)
{
  return _L4_msg_msg_tag (msg);
}


static inline void
_L4_attribute_always_inline
l4_set_msg_msg_tag (l4_msg_t msg, l4_msg_tag_t tag)
{
  _L4_set_msg_msg_tag (msg, tag);
}


static inline l4_word_t
_L4_attribute_always_inline
l4_msg_label (l4_msg_t msg)
{
  return _L4_msg_msg_label (msg);
}


static inline void
_L4_attribute_always_inline
l4_set_msg_label (l4_msg_t msg, l4_word_t label)
{
  _L4_set_msg_msg_label (msg, label);
}


static inline void
_L4_attribute_always_inline
l4_msg_load (l4_msg_t msg)
{
  _L4_msg_load (msg);
}


static inline void
_L4_attribute_always_inline
l4_msg_store (l4_msg_tag_t tag, l4_msg_t msg)
{
  _L4_msg_store (tag, msg);
}


static inline void
_L4_attribute_always_inline
l4_msg_clear (l4_msg_t msg)
{
  _L4_msg_clear (msg);
}


static inline void
_L4_attribute_always_inline
l4_msg_append_word (l4_msg_t msg, l4_word_t data)
{
  _L4_msg_append_word (msg, data);
}


static inline void
_L4_attribute_always_inline
l4_msg_append_map_item (l4_msg_t msg, l4_map_item_t map_item)
{
  _L4_msg_append_map_item (msg, map_item);
}


static inline void
_L4_attribute_always_inline
l4_msg_append_grant_item (l4_msg_t msg, l4_grant_item_t grant_item)
{
  _L4_msg_append_grant_item (msg, grant_item);
}


static inline void
_L4_attribute_always_inline
l4_msg_append_simple_string_item (l4_msg_t msg,
				  l4_string_item_t string_item)
{
  _L4_msg_append_simple_string_item (msg, string_item);
}


static inline void
_L4_attribute_always_inline
l4_msg_append_string_item (l4_msg_t msg, l4_string_item_t *string_item)
{
  _L4_msg_append_string_item (msg, string_item);
}


static inline void
_L4_attribute_always_inline
l4_msg_put_word (l4_msg_t msg, l4_word_t nr, l4_word_t data)
{
  _L4_msg_put_word (msg, nr, data);
}


static inline void
_L4_attribute_always_inline
l4_msg_put_map_item (l4_msg_t msg, l4_word_t nr, l4_map_item_t map_item)
{
  _L4_msg_put_map_item (msg, nr, map_item);
}


static inline void
_L4_attribute_always_inline
l4_msg_put_grant_item (l4_msg_t msg, l4_word_t nr, l4_grant_item_t grant_item)
{
  _L4_msg_put_grant_item (msg, nr, grant_item);
}


static inline void
_L4_attribute_always_inline
l4_msg_put_simple_string_item (l4_msg_t msg, l4_word_t nr,
			       l4_string_item_t string_item)
{
  _L4_msg_put_simple_string_item (msg, nr, string_item);
}


static inline void
_L4_attribute_always_inline
l4_msg_put_string_item (l4_msg_t msg, l4_word_t nr,
			l4_string_item_t *string_item)
{
  _L4_msg_put_string_item (msg, nr, string_item);
}


static inline l4_word_t
_L4_attribute_always_inline
l4_msg_word (l4_msg_t msg, l4_word_t nr)
{
  return _L4_msg_word (msg, nr);
}


static inline void
_L4_attribute_always_inline
l4_msg_get_word (l4_msg_t msg, l4_word_t nr,
				    l4_word_t *data)
{
  _L4_msg_get_word (msg, nr, data);
}


static inline l4_word_t
_L4_attribute_always_inline
l4_msg_get_map_item (l4_msg_t msg, l4_word_t nr, l4_map_item_t *map_item)
{
  return _L4_msg_get_map_item (msg, nr, map_item);
}


static inline l4_word_t
_L4_attribute_always_inline
l4_msg_get_grant_item (l4_msg_t msg, l4_word_t nr, l4_grant_item_t *grant_item)
{
  return _L4_msg_get_grant_item (msg, nr, grant_item);
}


static inline l4_word_t
_L4_attribute_always_inline
l4_msg_get_string_item (l4_msg_t msg, l4_word_t nr,
			l4_string_item_t *string_item)
{
  return _L4_msg_get_string_item (msg, nr, string_item);
}



/* IPC interface.  */

static inline l4_word_t
_L4_attribute_always_inline
l4_timeouts (l4_time_t send_timeout, l4_time_t receive_timeout)
{
  return _L4_timeouts (send_timeout, receive_timeout);
}


static inline l4_msg_tag_t
_L4_attribute_always_inline
l4_call_timeouts (l4_thread_id_t dest, l4_time_t send_timeout,
		  l4_time_t receive_timeout)
{
  return _L4_call_timeouts (dest, send_timeout, receive_timeout);
}


static inline l4_msg_tag_t
_L4_attribute_always_inline
l4_call (l4_thread_id_t dest)
{
  return _L4_call (dest);
}


static inline l4_msg_tag_t
_L4_attribute_always_inline
l4_send_timeout (l4_thread_id_t dest, l4_time_t send_timeout)
{
  return _L4_send_timeout (dest, send_timeout);
}


static inline l4_msg_tag_t
_L4_attribute_always_inline
l4_send (l4_thread_id_t dest)
{
  return _L4_send (dest);
}


static inline l4_msg_tag_t
_L4_attribute_always_inline
l4_reply (l4_thread_id_t dest)
{
  return _L4_reply (dest);
}


static inline l4_msg_tag_t
_L4_attribute_always_inline
l4_receive_timeout (l4_thread_id_t from, l4_time_t receive_timeout)
{
  return _L4_receive_timeout (from, receive_timeout);
}


static inline l4_msg_tag_t
_L4_attribute_always_inline
l4_receive (l4_thread_id_t from)
{
  return _L4_receive (from);
}


static inline l4_msg_tag_t
_L4_attribute_always_inline
l4_wait_timeout (l4_time_t receive_timeout, l4_thread_id_t *from)
{
  return _L4_wait_timeout (receive_timeout, from);
}


static inline l4_msg_tag_t
_L4_attribute_always_inline
l4_wait (l4_thread_id_t *from)
{
  return _L4_wait (from);
}


static inline l4_msg_tag_t
_L4_attribute_always_inline
l4_reply_wait_timeout (l4_thread_id_t dest, l4_time_t receive_timeout,
		       l4_thread_id_t *from)
{
  return _L4_reply_wait_timeout (dest, receive_timeout, from);
}


static inline l4_msg_tag_t
_L4_attribute_always_inline
l4_reply_wait (l4_thread_id_t dest, l4_thread_id_t *from)
{
  return _L4_reply_wait (dest, from);
}


static inline void
_L4_attribute_always_inline
l4_sleep (l4_time_t time)
{
  _L4_sleep (time);
}


static inline l4_msg_tag_t
_L4_attribute_always_inline
l4_lcall (l4_thread_id_t dest)
{
  return _L4_lcall (dest);
}


static inline l4_msg_tag_t
_L4_attribute_always_inline
l4_lreply_wait (l4_thread_id_t dest, l4_thread_id_t *from)
{
  return _L4_lreply_wait (dest, from);
}


static inline l4_word_t
_L4_attribute_always_inline
l4_ipc_succeeded (l4_msg_tag_t tag)
{
  return _L4_ipc_succeeded (tag);
}


static inline l4_word_t
_L4_attribute_always_inline
l4_ipc_failed (l4_msg_tag_t tag)
{
  return _L4_ipc_failed (tag);
}


static inline l4_word_t
_L4_attribute_always_inline
l4_ipc_propagated (l4_msg_tag_t tag)
{
  return _L4_ipc_propagated (tag);
}


static inline l4_word_t
_L4_attribute_always_inline
l4_ipc_redirected (l4_msg_tag_t tag)
{
  return _L4_ipc_redirected (tag);
}


static inline l4_word_t
_L4_attribute_always_inline
l4_ipc_xcpu (l4_msg_tag_t tag)
{
  return _L4_ipc_xcpu (tag);
}


static inline void
_L4_attribute_always_inline
l4_set_propagation (l4_msg_tag_t *tag)
{
  return _L4_set_propagation (tag);
}
