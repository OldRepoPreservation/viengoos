/* ipc.h - Public interface to the L4 IPC primitive.
   Copyright (C) 2003 Free Software Foundation, Inc.
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
#define _L4_IPC_H	1

#include <l4/types.h>
#include <l4/bits/ipc.h>
#include <l4/vregs.h>
#include <l4/syscall.h>
#include <l4/schedule.h>

/* Message tags.  */

#define l4_niltag	((l4_msg_tag_t) { .raw = 0 })


static inline l4_word_t
__attribute__((__always_inline__))
l4_is_msg_tag_equal (l4_msg_tag_t tag1, l4_msg_tag_t tag2)
{
  return tag1.raw == tag2.raw;
}


static inline l4_word_t
__attribute__((__always_inline__))
l4_is_msg_tag_not_equal (l4_msg_tag_t tag1, l4_msg_tag_t tag2)
{
  return tag1.raw != tag2.raw;
}


static inline l4_word_t
__attribute__((__always_inline__))
l4_label (l4_msg_tag_t tag)
{
  return tag.label;
}


static inline l4_word_t
__attribute__((__always_inline__))
l4_untyped_words (l4_msg_tag_t tag)
{
  return tag.untyped;
}


static inline l4_word_t
__attribute__((__always_inline__))
l4_typed_words (l4_msg_tag_t tag)
{
  return tag.typed;
}


static inline l4_msg_tag_t
__attribute__((__always_inline__))
l4_msg_tag_add_label (l4_msg_tag_t tag, l4_word_t label)
{
  l4_msg_tag_t new_tag = tag;
  new_tag.label = label;
  return new_tag;
}


static inline void
__attribute__((__always_inline__))
l4_msg_tag_add_label_to (l4_msg_tag_t *tag, l4_word_t label)
{
  tag->label = label;
}


static inline l4_msg_tag_t
__attribute__((__always_inline__))
l4_msg_tag (void)
{
  l4_msg_tag_t tag;
  l4_store_mr (0, &tag.raw);
  return tag;
}

static inline void
__attribute__((__always_inline__))
l4_set_msg_tag (l4_msg_tag_t tag)
{
  l4_load_mr (0, tag.raw);
}


/* Map items.  */

typedef union
{
  l4_word_t raw[2];
  struct
  {
    _L4_BITFIELD4
    (l4_word_t,
     _L4_BITFIELD (not_last, 1),
     _L4_BITFIELD (_four, 3),
     _L4_BITFIELD (_zero, 6),
     _L4_BITFIELD_32_64 (send_base, 22, 54));

    l4_fpage_t send_fpage;
  };
} l4_map_item_t;


static inline l4_map_item_t
__attribute__((__always_inline__))
l4_map_item (l4_fpage_t fpage, l4_word_t send_base)
{
  l4_map_item_t map_item;
  map_item.not_last = 0;
  map_item._four = 4;
  map_item._zero = 0;
  map_item.send_base = send_base / 1024;
  map_item.send_fpage = fpage;
  return map_item;
}


static inline l4_word_t
__attribute__((__always_inline__))
l4_is_map_item (l4_map_item_t map_item)
{
  return map_item._four == 4;
}


/* Grant items.  */

typedef union
{
  l4_word_t raw[2];
  struct
  {
    _L4_BITFIELD4
    (l4_word_t,
     _L4_BITFIELD (not_last, 1),
     _L4_BITFIELD (_five, 3),
     _L4_BITFIELD (_zero, 6),
     _L4_BITFIELD_32_64 (send_base, 22, 54));

    l4_fpage_t send_fpage;
  };
} l4_grant_item_t;


static inline l4_grant_item_t
__attribute__((__always_inline__))
l4_grant_item (l4_fpage_t fpage, l4_word_t send_base)
{
  l4_grant_item_t grant_item;
  grant_item.not_last = 0;
  grant_item._five = 5;
  grant_item._zero = 0;
  grant_item.send_base = send_base / 1024;
  grant_item.send_fpage = fpage;
  return grant_item;
}


static inline l4_word_t
__attribute__((__always_inline__))
l4_is_grant_item (l4_grant_item_t grant_item)
{
  return grant_item._five == 5;
}


/* String items.  */

typedef union
{
  l4_word_t raw[2];
  struct
  {
    _L4_BITFIELD6
    (l4_word_t,
     _L4_BITFIELD (not_last, 1),
     _L4_BITFIELD (cache_hint, 2),
     _L4_BITFIELD (_zero, 1),
     _L4_BITFIELD (nr_substrings, 5),
     _L4_BITFIELD (cont, 1),
     _L4_BITFIELD_32_64 (length, 22, 54));

    l4_word_t string[1];
  };
} l4_string_item_t;


typedef struct
{
  l4_word_t cache_hint;
} l4_cache_allocation_hint_t;

#define l4_use_default_cache_line_allocation \
  ((l4_cache_allocation_hint_t) { .cache_hint = 0 })


static inline l4_string_item_t
__attribute__((__always_inline__))
l4_string_item (int length, void *address)
{
  l4_string_item_t string_item;
  string_item.not_last = 0;
  string_item.cache_hint = l4_use_default_cache_line_allocation.cache_hint;
  string_item._zero = 0;
  string_item.nr_substrings = 0;
  string_item.cont = 0;
  string_item.length = length;
  string_item.string[0] = (l4_word_t) address;
  return string_item;
}


static inline l4_word_t
__attribute__((__always_inline__))
l4_is_string_item (l4_string_item_t *string_item)
{
  return string_item->_zero == 0;
}


static inline l4_word_t
__attribute__((__always_inline__))
l4_compound_string (l4_string_item_t *string_item)
{
  return string_item->cont;
}


static inline l4_word_t
__attribute__((__always_inline__))
l4_substrings (l4_string_item_t *string_item)
{
  return string_item->nr_substrings + 1;
}


static inline void *
__attribute__((__always_inline__))
l4_substring (l4_string_item_t *string_item, l4_word_t nr)
{
  return (void *) string_item->string[nr];
}


/* Append the string described by string item SOURCE to the string
   described by string item STRING_ITEM.  */
static inline l4_string_item_t *
__attribute__((__always_inline__))
l4_add_substring_address_to (l4_string_item_t *string_item,
			     l4_string_item_t *source)
{
  l4_string_item_t *target = string_item;
  int cont;

  /* First search for end of target string item.  */
  do
    {
      cont = target->cont;
      if (!cont)
	target->cont = 1;
      target = (l4_string_item_t *)
	(((void *) target) + 1 + target->nr_substrings + 1);
    }
  while (cont);

  /* Now copy the source string item.  */
  do
    {
      int nr = 1 + source->nr_substrings + 1;
      cont = source->cont;
      while (nr--)
	*(target++) = *(source++);
    }
  while (cont);

  return string_item;
}


/* Append the string described by string item SOURCE as a substring
   (of the same length) to the string item STRING_ITEM.  */
static inline l4_string_item_t *
__attribute__((__always_inline__))
l4_add_substring_to (l4_string_item_t *string_item, void *source)
{
  l4_string_item_t *target = string_item;

  /* First search for end of target string item.  */
  while (target->cont)
    target = (l4_string_item_t *)
      (((void *) target) + 1 + target->nr_substrings + 1);

  /* Now add the source substring.  */
  target->nr_substrings++;
  target->string[target->nr_substrings + 1] = (l4_word_t) source;

  return string_item;
}


static inline l4_word_t
__attribute__((__always_inline__))
l4_is_cache_allocation_hint_equal (l4_cache_allocation_hint_t hint1,
				   l4_cache_allocation_hint_t hint2)
{
  return hint1.cache_hint == hint2.cache_hint;
}


static inline l4_word_t
__attribute__((__always_inline__))
l4_is_cache_allocation_hint_not_equal (l4_cache_allocation_hint_t hint1,
				       l4_cache_allocation_hint_t hint2)
{
  return hint1.cache_hint != hint2.cache_hint;
}


static inline l4_cache_allocation_hint_t
__attribute__((__always_inline__))
l4_cache_allocation_hint (l4_string_item_t *string_item)
{
  l4_cache_allocation_hint_t hint;
  hint.cache_hint = string_item->cache_hint;
  return hint;
}


static inline l4_string_item_t
__attribute__((__always_inline__))
l4_add_cache_allocation_hint (l4_string_item_t string_item,
			      l4_cache_allocation_hint_t hint)
{
  string_item.cache_hint = hint.cache_hint;
  return string_item;
}


static inline l4_string_item_t *
__attribute__((__always_inline__))
l4_add_cache_allocation_hint_to (l4_string_item_t *string_item,
				 l4_cache_allocation_hint_t hint)
{
  string_item->cache_hint = hint.cache_hint;
  return string_item;
}


/* Acceptors and message buffers.  */
typedef _L4_RAW (l4_word_t, _L4_STRUCT1 ({
  _L4_BITFIELD3
    (l4_word_t,
     _L4_BITFIELD (string_items, 1),
     _L4_BITFIELD (_zero, 3),
     _L4_BITFIELD_32_64 (rcv_window, 28, 60));
})) l4_acceptor_t;

#define l4_untyped_words_acceptor \
  ((l4_acceptor_t) { .string_items = 0, ._zero = 0, .rcv_window = 0 })
#define l4_string_items_acceptor \
  ((l4_acceptor_t) { .string_items = 1, ._zero = 0, .rcv_window = 0 })


typedef union
{
  l4_word_t br[33];
  l4_string_item_t string_item[33];
} l4_msg_buffer_t;


static inline l4_acceptor_t
__attribute__((__always_inline__))
l4_map_grant_items (l4_fpage_t rcv_window)
{
  l4_acceptor_t acceptor;
  acceptor.raw = rcv_window.raw;
  acceptor.string_items = 1;
  acceptor._zero = 0;
  return acceptor;
}


static inline l4_acceptor_t
__attribute__((__always_inline__))
l4_add_acceptor (l4_acceptor_t acceptor1, l4_acceptor_t acceptor2)
{
  acceptor1.string_items |= acceptor2.string_items;
  if (acceptor2.rcv_window)
    acceptor1.rcv_window = acceptor2.rcv_window;
  return acceptor1;
}


static inline l4_acceptor_t *
__attribute__((__always_inline__))
l4_add_acceptor_to (l4_acceptor_t *acceptor1, l4_acceptor_t acceptor2)
{
  acceptor1->string_items |= acceptor2.string_items;
  if (acceptor2.rcv_window)
    acceptor1->rcv_window = acceptor2.rcv_window;
  return acceptor1;
}


static inline l4_acceptor_t
__attribute__((__always_inline__))
l4_remove_acceptor (l4_acceptor_t acceptor1, l4_acceptor_t acceptor2)
{
  acceptor1.string_items &= ~acceptor2.string_items;
  if (acceptor2.rcv_window)
    acceptor1.rcv_window = 0;
  return acceptor1;
}


static inline l4_acceptor_t *
__attribute__((__always_inline__))
l4_remove_acceptor_from (l4_acceptor_t *acceptor1, l4_acceptor_t acceptor2)
{
  acceptor1->string_items &= ~acceptor2.string_items;
  if (acceptor2.rcv_window)
    acceptor1->rcv_window = 0;
  return acceptor1;
}


static inline l4_word_t
__attribute__((__always_inline__))
l4_has_string_items (l4_acceptor_t acceptor)
{
  return acceptor.string_items;
}


static inline l4_word_t
__attribute__((__always_inline__))
l4_has_map_grant_items (l4_acceptor_t acceptor)
{
  return acceptor.rcv_window;
}


static inline l4_fpage_t
__attribute__((__always_inline__))
l4_rcv_window (l4_acceptor_t acceptor)
{
  l4_fpage_t fpage;
  acceptor.string_items = 0;
  fpage.raw = acceptor.raw;
  return fpage;
}


static inline void
__attribute__((__always_inline__))
l4_accept (l4_acceptor_t acceptor)
{
  l4_load_br (0, acceptor.raw);
}


static inline void
__attribute__((__always_inline__))
l4_accept_strings (l4_acceptor_t acceptor, l4_msg_buffer_t *msg_buffer)
{
  l4_string_item_t *string_item = msg_buffer->string_item;
  int br = 1;
  int cont;

  l4_load_br (0, acceptor.raw);
  do
    {
      int nr = 1 + string_item->nr_substrings + 1;
      cont = string_item->cont || string_item->not_last;
      l4_load_brs (br, nr, (l4_word_t *) string_item);
      br += nr;
      string_item = (l4_string_item_t *) (((void *) string_item) + nr);
    }
  while (cont);
}


static inline l4_acceptor_t
__attribute__((__always_inline__))
l4_accepted (void)
{
  l4_acceptor_t acceptor;
  l4_store_br (0, &acceptor.raw);
  return acceptor;
}


static inline void
__attribute__((__always_inline__))
l4_msg_buffer_clear (l4_msg_buffer_t *msg_buffer)
{
  msg_buffer->br[0] = 0;
}


static inline void
__attribute__((__always_inline__))
l4_msg_buffer_append_simple_rcv_string (l4_msg_buffer_t *msg_buffer,
					l4_string_item_t string_item)
{
  l4_string_item_t *target = msg_buffer->string_item;
  int cont;

  do
    {
      int nr = 1 + target->nr_substrings + 1;
      cont = target->cont | target->not_last;
      if (!cont)
	target->not_last = 1;
      target = (l4_string_item_t *) (((void *) target) + nr);
    }
  while (cont);

  string_item.not_last = 0;
  string_item.nr_substrings = 0;
  *target = string_item;
}


static inline void
__attribute__((__always_inline__))
l4_msg_buffer_append_rcv_string (l4_msg_buffer_t *msg_buffer,
				 l4_string_item_t *string_item)
{
  l4_string_item_t *target = msg_buffer->string_item;
  l4_word_t *brp;
  int cont;

  /* Find the end of the message buffer.  */
  do
    {
      int nr = 1 + target->nr_substrings + 1;
      cont = target->cont | target->not_last;
      if (!cont)
	target->not_last = 1;
      target = (l4_string_item_t *) (((void *) target) + nr);
    }
  while (cont);

  brp = (l4_word_t *) target;

  /* Copy the source string.  */
  do
    {
      int nr = string_item->nr_substrings + 1;
      l4_word_t *substrings = string_item->string;

      cont = string_item->cont;
      *(brp++) = string_item->raw[0];
      while (nr-- > 0)
	*(brp++) = *(substrings++);
      string_item = (l4_string_item_t *) substrings;
    }
  while (cont);
}


/* Message composition.  */

typedef union
{
  l4_msg_tag_t tag;
  l4_word_t mr[64];
} l4_msg_t;


static inline void
__attribute__((__always_inline__))
l4_msg_put (l4_msg_t *msg, l4_word_t label, int untyped_nr, l4_word_t *untyped,
	    int typed_nr, l4_word_t *typed)
{
  l4_word_t *mrs = msg->mr;

  msg->tag.untyped = untyped_nr;
  msg->tag.typed = typed_nr;
  msg->tag.propagated = 0;
  msg->tag.redirected = 0;
  msg->tag.cross_cpu = 0;
  msg->tag.error = 0;
  msg->tag.label = label;

  while (untyped_nr--)
    *(mrs++) = *(untyped++);

  while (typed_nr--)
    *(mrs++) = *(typed++);
}


static inline void
__attribute__((__always_inline__))
l4_msg_get (l4_msg_t *msg, l4_word_t *untyped, l4_word_t *typed)
{
  l4_word_t *mr = msg->mr;
  int untyped_nr = msg->tag.untyped;
  int typed_nr = msg->tag.typed;

  while (untyped_nr)
    *(untyped++) = *(mr++);

  while (typed_nr)
    *(typed++) = *(mr++);
}


static inline l4_msg_tag_t
__attribute__((__always_inline__))
l4_msg_msg_tag (l4_msg_t *msg)
{
  return msg->tag;
}


static inline void
__attribute__((__always_inline__))
l4_set_msg_msg_tag (l4_msg_t *msg, l4_msg_tag_t tag)
{
  msg->tag = tag;
}


static inline l4_word_t
__attribute__((__always_inline__))
l4_msg_label (l4_msg_t *msg)
{
  return msg->tag.label;
}


static inline void
__attribute__((__always_inline__))
l4_set_msg_label (l4_msg_t *msg, l4_word_t label)
{
  msg->tag.label = label;
}


static inline void
__attribute__((__always_inline__))
l4_msg_load (l4_msg_t *msg)
{
  l4_load_mrs (0, 1 + msg->tag.untyped + msg->tag.typed, msg->mr);
}


static inline void
__attribute__((__always_inline__))
l4_msg_store (l4_msg_tag_t tag, l4_msg_t *msg)
{
  msg->tag = tag;
  l4_store_mrs (1, msg->tag.untyped + msg->tag.typed, &msg->mr[1]);
}


static inline void
__attribute__((__always_inline__))
l4_msg_clear (l4_msg_t *msg)
{
  msg->tag = l4_niltag;
}


static inline void
__attribute__((__always_inline__))
l4_msg_append_word (l4_msg_t *msg, l4_word_t data)
{
  l4_word_t new_untyped_nr = ++msg->tag.untyped;
  l4_word_t typed_nr = msg->tag.typed;

  if (typed_nr)
    {
      l4_word_t *mrs = &msg->mr[new_untyped_nr + typed_nr];
      while (typed_nr--)
	{
	  *(mrs) = *(mrs - 1);
	  mrs--;
	}
      *mrs = data;
    }
  else
    msg->mr[new_untyped_nr] = data;
}


static inline void
__attribute__((__always_inline__))
l4_msg_append_map_item (l4_msg_t *msg, l4_map_item_t map_item)
{
  /* The "not last" bit is ignored for sending.  */
  int pos = 1 + msg->tag.untyped + msg->tag.typed;
  msg->mr[pos++] = map_item.raw[0];
  msg->mr[pos] = map_item.raw[1];
  msg->tag.typed += 2;
}
    
  
static inline void
__attribute__((__always_inline__))
l4_msg_append_grant_item (l4_msg_t *msg, l4_grant_item_t grant_item)
{
  /* The "not last" bit is ignored for sending.  */
  int pos = 1 + msg->tag.untyped + msg->tag.typed;
  msg->mr[pos++] = grant_item.raw[0];
  msg->mr[pos] = grant_item.raw[1];
  msg->tag.typed += 2;
}
    
  
static inline void
__attribute__((__always_inline__))
l4_msg_append_simple_string_item (l4_msg_t *msg, l4_string_item_t string_item)
{
  /* The "not last" bit is ignored for sending.  */
  int pos = 1 + msg->tag.untyped + msg->tag.typed;
  string_item.cont = 0;
  string_item.nr_substrings = 0;
  msg->mr[pos++] = string_item.raw[0];
  msg->mr[pos] = string_item.raw[1];
  msg->tag.typed += 2;
}
    
  
static inline void
__attribute__((__always_inline__))
l4_msg_append_string_item (l4_msg_t *msg, l4_string_item_t *string_item)
{
  /* The "not last" bit is ignored for sending.  */
  int pos = 1 + msg->tag.untyped + msg->tag.typed;
  int cont;

  do
    {
      int nr = string_item->nr_substrings + 1;
      l4_word_t *substrings = string_item->string;

      cont = string_item->cont;
      msg->mr[pos++] = string_item->raw[0];
      while (nr-- > 0)
	msg->mr[pos++] = *(substrings++);
      string_item = (l4_string_item_t *) substrings;
    }
  while (cont);

  msg->tag.typed += pos - 1 - msg->tag.untyped;
}


static inline void
__attribute__((__always_inline__))
l4_msg_put_word (l4_msg_t *msg, l4_word_t nr, l4_word_t data)
{
  msg->mr[1 + nr] = data;
}

static inline void
__attribute__((__always_inline__))
l4_msg_put_map_item (l4_msg_t *msg, l4_word_t nr, l4_map_item_t map_item)
{
  l4_word_t pos = 1 + msg->tag.untyped + nr;
  msg->mr[pos++] = map_item.raw[0];
  msg->mr[pos] = map_item.raw[1];
}


static inline void
__attribute__((__always_inline__))
l4_msg_put_grant_item (l4_msg_t *msg, l4_word_t nr, l4_grant_item_t grant_item)
{
  l4_word_t pos = 1 + msg->tag.untyped + nr;
  msg->mr[pos++] = grant_item.raw[0];
  msg->mr[pos] = grant_item.raw[1];
}


static inline void
__attribute__((__always_inline__))
l4_msg_put_simple_string_item (l4_msg_t *msg, l4_word_t nr,
			       l4_string_item_t string_item)
{
  l4_word_t pos = 1 + msg->tag.untyped + nr;
  string_item.cont = 0;
  string_item.nr_substrings = 0;
  msg->mr[pos++] = string_item.raw[0];
  msg->mr[pos] = string_item.raw[1];
}


static inline void
__attribute__((__always_inline__))
l4_msg_put_string_item (l4_msg_t *msg, l4_word_t nr,
			l4_string_item_t *string_item)
{
  l4_word_t pos = 1 + msg->tag.untyped + nr;
  int cont;

  do
    {
      int nr = string_item->nr_substrings + 1;
      l4_word_t *substrings = string_item->string;

      cont = string_item->cont;
      msg->mr[pos++] = string_item->raw[0];
      while (nr-- > 0)
	msg->mr[pos++] = *(substrings++);
      string_item = (l4_string_item_t *) substrings;
    }
  while (cont);
}


static inline l4_word_t
__attribute__((__always_inline__))
l4_msg_word (l4_msg_t *msg, l4_word_t nr)
{
  return msg->mr[1 + nr];
}


static inline void
__attribute__((__always_inline__))
l4_msg_get_word (l4_msg_t *msg, l4_word_t nr, l4_word_t *data)
{
  *data = l4_msg_word (msg, nr);
}


static inline void
__attribute__((__always_inline__))
l4_msg_get_map_item (l4_msg_t *msg, l4_word_t nr, l4_map_item_t *map_item)
{
  l4_word_t pos = 1 + msg->tag.untyped + nr;
  map_item->raw[0] = msg->mr[pos++];
  map_item->raw[1] = msg->mr[pos];
}


static inline void
__attribute__((__always_inline__))
l4_msg_get_grant_item (l4_msg_t *msg, l4_word_t nr,
		       l4_grant_item_t *grant_item)
{
  l4_word_t pos = 1 + msg->tag.untyped + nr;
  grant_item->raw[0] = msg->mr[pos++];
  grant_item->raw[1] = msg->mr[pos];
}


static inline void
__attribute__((__always_inline__))
l4_msg_get_string_item (l4_msg_t *msg, l4_word_t nr,
			l4_string_item_t *string_item)
{
  l4_word_t pos = 1 + msg->tag.untyped + nr;
  int cont;

  do
    {
      int nr = string_item->nr_substrings + 1;
      l4_word_t *substrings = string_item->string;

      cont = string_item->cont;
      msg->mr[pos++] = string_item->raw[0];
      while (nr-- > 0)
	*(substrings++) = msg->mr[pos++];
      string_item = (l4_string_item_t *) substrings;
    }
  while (cont);
}


/* l4_ipc convenience interface.  */

static inline l4_word_t
__attribute__((__always_inline__))
l4_timeouts (l4_time_t send_timeout, l4_time_t receive_timeout)
{
  return (send_timeout.raw << 16) | receive_timeout.raw;
}


static inline l4_msg_tag_t
__attribute__((__always_inline__))
l4_call_timeouts (l4_thread_id_t dest, l4_time_t send_timeout,
		  l4_time_t receive_timeout)
{
  l4_thread_id_t from;
  return l4_ipc (dest, dest, l4_timeouts (send_timeout, receive_timeout),
		 &from);
}


static inline l4_msg_tag_t
__attribute__((__always_inline__))
l4_call (l4_thread_id_t dest)
{
  return l4_call_timeouts (dest, l4_never, l4_never);
}


static inline l4_msg_tag_t
__attribute__((__always_inline__))
l4_send_timeout (l4_thread_id_t dest, l4_time_t send_timeout)
{
  l4_thread_id_t dummy;
  return l4_ipc (dest, l4_nilthread, l4_timeouts (send_timeout, l4_zero_time),
		 &dummy);
}


static inline l4_msg_tag_t
__attribute__((__always_inline__))
l4_send (l4_thread_id_t dest)
{
  return l4_send_timeout (dest, l4_never);
}


static inline l4_msg_tag_t
__attribute__((__always_inline__))
l4_reply (l4_thread_id_t dest)
{
  return l4_send_timeout (dest, l4_zero_time);
}


static inline l4_msg_tag_t
__attribute__((__always_inline__))
l4_receive_timeout (l4_thread_id_t from, l4_time_t receive_timeout)
{
  l4_thread_id_t dummy;
  return l4_ipc (l4_nilthread, from,
		 l4_timeouts (l4_zero_time, receive_timeout), &dummy);
}


static inline l4_msg_tag_t
__attribute__((__always_inline__))
l4_receive (l4_thread_id_t from)
{
  return l4_receive_timeout (from, l4_never);
}


static inline l4_msg_tag_t
__attribute__((__always_inline__))
l4_wait_timeout (l4_time_t receive_timeout, l4_thread_id_t *from)
{
  return l4_ipc (l4_nilthread, l4_anythread,
		 l4_timeouts (l4_zero_time, receive_timeout), from);
}


static inline l4_msg_tag_t
__attribute__((__always_inline__))
l4_wait (l4_thread_id_t *from)
{
  return l4_wait_timeout (l4_never, from);
}


static inline l4_msg_tag_t
__attribute__((__always_inline__))
l4_reply_wait_timeout (l4_thread_id_t dest, l4_time_t receive_timeout,
		       l4_thread_id_t *from)
{
  return l4_ipc (dest, l4_anythread,
		 l4_timeouts (l4_time_period (0), receive_timeout), from);
}


static inline l4_msg_tag_t
__attribute__((__always_inline__))
l4_reply_wait (l4_thread_id_t dest, l4_thread_id_t *from)
{
  return l4_reply_wait_timeout (dest, l4_never, from);
}


static inline void
__attribute__((__always_inline__))
l4_sleep (l4_time_t time)
{
  l4_set_msg_tag (l4_receive_timeout (l4_my_local_id (), time));
}


static inline l4_msg_tag_t
__attribute__((__always_inline__))
l4_lcall (l4_thread_id_t dest)
{
  l4_thread_id_t dummy;
  return l4_lipc (dest, dest, l4_timeouts (l4_never, l4_never), &dummy);
}


static inline l4_msg_tag_t
__attribute__((__always_inline__))
l4_lreply_wait (l4_thread_id_t dest, l4_thread_id_t *from)
{
  return l4_lipc (dest, dest, l4_timeouts (l4_time_period (0), l4_never),
		  from);
}


static inline l4_word_t
__attribute__((__always_inline__))
l4_ipc_succeeded (l4_msg_tag_t tag)
{
  return !tag.error;
}


static inline l4_word_t
__attribute__((__always_inline__))
l4_ipc_failed (l4_msg_tag_t tag)
{
  return tag.error;
}


static inline l4_word_t
__attribute__((__always_inline__))
l4_ipc_propagated (l4_msg_tag_t tag)
{
  return tag.propagated;
}


static inline l4_word_t
__attribute__((__always_inline__))
l4_ipc_redirected (l4_msg_tag_t tag)
{
  return tag.redirected;
}


static inline l4_word_t
__attribute__((__always_inline__))
l4_ipc_xcpu (l4_msg_tag_t tag)
{
  return tag.cross_cpu;
}


static inline void
__attribute__((__always_inline__))
l4_set_propagation (l4_msg_tag_t *tag)
{
  tag->propagated = 1;
}

#endif	/* l4/ipc.h */
