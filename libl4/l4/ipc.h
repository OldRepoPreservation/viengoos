/* l4/ipc.h - Public interface to the L4 IPC primitive.
   Copyright (C) 2003, 2004 Free Software Foundation, Inc.
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

#include <l4/features.h>
#include <l4/types.h>
#include <l4/bits/ipc.h>
#include <l4/vregs.h>
#include <l4/syscall.h>
#include <l4/schedule.h>
#include <l4/message.h>

/* Must make visible the thread control registers related to IPC.  */
#include <l4/thread.h>


/* Message tags.  */

/* _L4_msg_tag_t is defined in <l4/types.h>.  */

typedef _L4_RAW (_L4_msg_tag_t, _L4_STRUCT1 ({
  _L4_BITFIELD7
    (_L4_word_t,
     _L4_BITFIELD (untyped, 6),
     _L4_BITFIELD (typed, 6),
     _L4_BITFIELD (propagated, 1),
     _L4_BITFIELD (redirected, 1),
     _L4_BITFIELD (cross_cpu, 1),
     _L4_BITFIELD (error, 1),
     _L4_BITFIELD_32_64 (label, 16, 48));
})) __L4_msg_tag_t;


#define _L4_niltag	((_L4_msg_tag_t) 0 )


static inline _L4_word_t
_L4_attribute_always_inline
_L4_is_msg_tag_equal (_L4_msg_tag_t tag1, _L4_msg_tag_t tag2)
{
  return tag1 == tag2;
}


static inline _L4_word_t
_L4_attribute_always_inline
_L4_is_msg_tag_not_equal (_L4_msg_tag_t tag1, _L4_msg_tag_t tag2)
{
  return tag1 != tag2;
}


static inline _L4_word_t
_L4_attribute_always_inline
_L4_label (_L4_msg_tag_t tag)
{
  __L4_msg_tag_t _tag;

  _tag.raw = tag;
  return _tag.label;
}


static inline _L4_word_t
_L4_attribute_always_inline
_L4_untyped_words (_L4_msg_tag_t tag)
{
  __L4_msg_tag_t _tag;

  _tag.raw = tag;
  return _tag.untyped;
}


static inline _L4_word_t
_L4_attribute_always_inline
_L4_typed_words (_L4_msg_tag_t tag)
{
  __L4_msg_tag_t _tag;

  _tag.raw = tag;
  return _tag.typed;
}


static inline _L4_msg_tag_t
_L4_attribute_always_inline
_L4_msg_tag_add_label (_L4_msg_tag_t tag, _L4_word_t label)
{
  __L4_msg_tag_t _tag;

  _tag.raw = tag;
  _tag.label = label;
  return _tag.raw;
}


static inline void
_L4_attribute_always_inline
_L4_msg_tag_add_label_to (_L4_msg_tag_t *tag, _L4_word_t label)
{
  __L4_msg_tag_t _tag;

  _tag.raw = *tag;
  _tag.label = label;
  *tag = _tag.raw;
}


static inline _L4_msg_tag_t
_L4_attribute_always_inline
_L4_msg_tag (void)
{
  _L4_msg_tag_t tag;
  _L4_store_mr (0, &tag);
  return tag;
}

static inline void
_L4_attribute_always_inline
_L4_set_msg_tag (_L4_msg_tag_t tag)
{
  _L4_load_mr (0, tag);
}


/* Map items.  */

typedef _L4_dword_t _L4_map_item_t;
 
typedef union
{
  _L4_map_item_t raw;

  /* We need the following member to avoid breaking the aliasing rules.  */
  l4_word_t mr[2];

  struct
  {
    _L4_BITFIELD4
    (_L4_word_t,
     _L4_BITFIELD (not_last, 1),
     _L4_BITFIELD (_four, 3),
     _L4_BITFIELD (_zero, 6),
     _L4_BITFIELD_32_64 (send_base, 22, 54));

    _L4_fpage_t send_fpage;
  };
} __L4_map_item_t;


static inline _L4_map_item_t
_L4_attribute_always_inline
_L4_map_item (_L4_fpage_t fpage, _L4_word_t send_base)
{
  __L4_map_item_t map_item;
  map_item.not_last = 0;
  map_item._four = 4;
  map_item._zero = 0;
  map_item.send_base = send_base >> 10;
  map_item.send_fpage = fpage;
  return map_item.raw;
}


static inline _L4_word_t
_L4_attribute_always_inline
_L4_is_map_item (_L4_map_item_t map_item)
{
  __L4_map_item_t _map_item;

  _map_item.raw = map_item;
  return _map_item._four == 4;
}


static inline _L4_word_t
_L4_attribute_always_inline
_L4_map_item_snd_fpage (_L4_map_item_t map_item)
{
  __L4_map_item_t _map_item;

  _map_item.raw = map_item;
  return _map_item.send_fpage;
}


static inline _L4_word_t
_L4_attribute_always_inline
_L4_map_item_snd_base (_L4_map_item_t map_item)
{
  __L4_map_item_t _map_item;

  _map_item.raw = map_item;
  return _map_item.send_base << 10;
}


/* Grant items.  */

typedef _L4_dword_t _L4_grant_item_t;

typedef union
{
  _L4_grant_item_t raw;

  /* We need the following member to avoid breaking the aliasing rules.  */
  _L4_word_t mr[2];

  struct
  {
    _L4_BITFIELD4
    (_L4_word_t,
     _L4_BITFIELD (not_last, 1),
     _L4_BITFIELD (_five, 3),
     _L4_BITFIELD (_zero, 6),
     _L4_BITFIELD_32_64 (send_base, 22, 54));

    _L4_fpage_t send_fpage;
  };
} __L4_grant_item_t;


static inline _L4_grant_item_t
_L4_attribute_always_inline
_L4_grant_item (_L4_fpage_t fpage, _L4_word_t send_base)
{
  __L4_grant_item_t grant_item;
  grant_item.not_last = 0;
  grant_item._five = 5;
  grant_item._zero = 0;
  grant_item.send_base = send_base / 1024;
  grant_item.send_fpage = fpage;
  return grant_item.raw;
}


static inline _L4_word_t
_L4_attribute_always_inline
_L4_is_grant_item (_L4_grant_item_t grant_item)
{
  __L4_grant_item_t _grant_item;
  _grant_item.raw = grant_item;
  return _grant_item._five == 5;
}


static inline _L4_word_t
_L4_attribute_always_inline
_L4_grant_item_snd_fpage (_L4_grant_item_t grant_item)
{
  __L4_grant_item_t _grant_item;

  _grant_item.raw = grant_item;
  return _grant_item.send_fpage;
}


static inline _L4_word_t
_L4_attribute_always_inline
_L4_grant_item_snd_base (_L4_grant_item_t grant_item)
{
  __L4_grant_item_t _grant_item;

  _grant_item.raw = grant_item;
  return _grant_item.send_base << 10;
}


/* String items.  */

typedef _L4_dword_t _L4_string_item_t;

typedef union
{
  _L4_string_item_t raw;

  /* We need the following member to avoid breaking the aliasing rules.  */
  l4_word_t mr[2];

  struct
  {
    _L4_BITFIELD6
    (_L4_word_t,
     _L4_BITFIELD (not_last, 1),
     _L4_BITFIELD (cache_hint, 2),
     _L4_BITFIELD (_zero, 1),
     _L4_BITFIELD (nr_substrings, 5),
     _L4_BITFIELD (cont, 1),
     _L4_BITFIELD_32_64 (length, 22, 54));

    _L4_word_t string[1];
  };
} __L4_string_item_t;


typedef _L4_word_t _L4_cache_allocation_hint_t;

#define _L4_use_default_cache_line_allocation ((_L4_cache_allocation_hint_t) 0)

static inline _L4_string_item_t
_L4_attribute_always_inline
_L4_string_item (int length, void *address)
{
  __L4_string_item_t string_item;
  string_item.not_last = 0;
  string_item.cache_hint = _L4_use_default_cache_line_allocation;
  string_item._zero = 0;
  string_item.nr_substrings = 0;
  string_item.cont = 0;
  string_item.length = length;
  string_item.string[0] = (_L4_word_t) address;
  return string_item.raw;
}


static inline _L4_word_t
_L4_attribute_always_inline
_L4_is_string_item (_L4_string_item_t *string_item)
{
  __L4_string_item_t *_string_item = (__L4_string_item_t *) string_item;

  return _string_item->_zero == 0;
}


static inline _L4_word_t
_L4_attribute_always_inline
_L4_compound_string (_L4_string_item_t *string_item)
{
  __L4_string_item_t *_string_item = (__L4_string_item_t *) string_item;

  return _string_item->cont;
}


static inline _L4_word_t
_L4_attribute_always_inline
_L4_substrings (_L4_string_item_t *string_item)
{
  __L4_string_item_t *_string_item = (__L4_string_item_t *) string_item;

  return _string_item->nr_substrings + 1;
}


static inline void *
_L4_attribute_always_inline
_L4_substring (_L4_string_item_t *string_item, _L4_word_t nr)
{
  __L4_string_item_t *_string_item = (__L4_string_item_t *) string_item;
  return (void *) _string_item->string[nr];
}


/* Append the string described by string item SOURCE to the string
   described by string item STRING_ITEM.  */
static inline _L4_string_item_t *
_L4_attribute_always_inline
_L4_add_substring_address_to (_L4_string_item_t *string_item,
			      _L4_string_item_t *source)
{
  __L4_string_item_t *target = (__L4_string_item_t *) string_item;
  __L4_string_item_t *_source = (__L4_string_item_t *) source;
  int cont;

  /* First search for end of target string item.  */
  do
    {
      cont = target->cont;
      if (!cont)
	target->cont = 1;
      target = (__L4_string_item_t *)
	(((_L4_word_t *) target) + 1 + target->nr_substrings + 1);
    }
  while (cont);

  /* Now copy the source string item.  */
  do
    {
      int nr = 1 + _source->nr_substrings + 1;
      cont = _source->cont;
      while (nr--)
	*(target++) = *(_source++);
    }
  while (cont);

  return string_item;
}


/* Append the string described by string item SOURCE as a substring
   (of the same length) to the string item STRING_ITEM.  */
static inline _L4_string_item_t *
_L4_attribute_always_inline
_L4_add_substring_to (_L4_string_item_t *string_item, void *source)
{
  __L4_string_item_t *target = (__L4_string_item_t *) string_item;

  /* First search for end of target string item.  */
  while (target->cont)
    target = (__L4_string_item_t *)
      (((_L4_word_t *) target) + 1 + target->nr_substrings + 1);

  /* Now add the source substring.  */
  target->nr_substrings++;
  target->string[target->nr_substrings + 1] = (_L4_word_t) source;

  return string_item;
}


static inline _L4_cache_allocation_hint_t
_L4_attribute_always_inline
_L4_cache_allocation_hint (_L4_string_item_t string_item)
{
  __L4_string_item_t _string_item;

  _string_item.raw = string_item;
  return _string_item.cache_hint;
}


static inline _L4_string_item_t
_L4_attribute_always_inline
_L4_add_cache_allocation_hint (_L4_string_item_t string_item,
			       _L4_cache_allocation_hint_t hint)
{
  __L4_string_item_t _string_item;

  _string_item.raw = string_item;
  _string_item.cache_hint = hint;
  return _string_item.raw;
}


static inline _L4_string_item_t *
_L4_attribute_always_inline
_L4_add_cache_allocation_hint_to (_L4_string_item_t *string_item,
				  _L4_cache_allocation_hint_t hint)
{
  __L4_string_item_t *_string_item = (__L4_string_item_t *) string_item;

  _string_item->cache_hint = hint;
  return string_item;
}


/* Acceptors and message buffers.  */

typedef _L4_word_t _L4_acceptor_t;

typedef _L4_RAW (_L4_acceptor_t, _L4_STRUCT1 ({
  _L4_BITFIELD3
    (_L4_word_t,
     _L4_BITFIELD (string_items, 1),
     _L4_BITFIELD (_zero, 3),
     _L4_BITFIELD_32_64 (rcv_window, 28, 60));
})) __L4_acceptor_t;

#define _L4_untyped_words_acceptor	((_L4_acceptor_t) 0)
/* For string items, only bit 1 is set.  */
#define _L4_string_items_acceptor	((_L4_acceptor_t) 1)

typedef _L4_word_t _L4_msg_buffer_t[_L4_NUM_BRS - 1];

static inline _L4_acceptor_t
_L4_attribute_always_inline
_L4_map_grant_items (_L4_fpage_t rcv_window)
{
  __L4_acceptor_t acceptor;
  acceptor.raw = rcv_window;
  acceptor.string_items = 0;
  acceptor._zero = 0;
  return acceptor.raw;
}


static inline _L4_acceptor_t
_L4_attribute_always_inline
_L4_add_acceptor (_L4_acceptor_t acceptor1, _L4_acceptor_t acceptor2)
{
  __L4_acceptor_t _acceptor1;
  __L4_acceptor_t _acceptor2;

  _acceptor1.raw = acceptor1;
  _acceptor2.raw = acceptor2;
  _acceptor1.string_items |= _acceptor2.string_items;
  if (_acceptor2.rcv_window)
    _acceptor1.rcv_window = _acceptor2.rcv_window;
  return _acceptor1.raw;
}


static inline _L4_acceptor_t *
_L4_attribute_always_inline
_L4_add_acceptor_to (_L4_acceptor_t *acceptor1, _L4_acceptor_t acceptor2)
{
  __L4_acceptor_t *_acceptor1 = (__L4_acceptor_t *) acceptor1;
  __L4_acceptor_t _acceptor2;

  _acceptor2.raw = acceptor2;
  _acceptor1->string_items |= _acceptor2.string_items;
  if (_acceptor2.rcv_window)
    _acceptor1->rcv_window = _acceptor2.rcv_window;
  return acceptor1;
}


static inline _L4_acceptor_t
_L4_attribute_always_inline
_L4_remove_acceptor (_L4_acceptor_t acceptor1, _L4_acceptor_t acceptor2)
{
  __L4_acceptor_t _acceptor1;
  __L4_acceptor_t _acceptor2;

  _acceptor1.raw = acceptor1;
  _acceptor2.raw = acceptor2;
  _acceptor1.string_items &= ~_acceptor2.string_items;
  if (_acceptor2.rcv_window)
    _acceptor1.rcv_window = 0;
  return _acceptor1.raw;
}


static inline _L4_acceptor_t *
_L4_attribute_always_inline
_L4_remove_acceptor_from (_L4_acceptor_t *acceptor1, _L4_acceptor_t acceptor2)
{
  __L4_acceptor_t *_acceptor1 = (__L4_acceptor_t *) acceptor1;
  __L4_acceptor_t _acceptor2;

  _acceptor2.raw = acceptor2;
  _acceptor1->string_items &= ~_acceptor2.string_items;
  if (_acceptor2.rcv_window)
    _acceptor1->rcv_window = 0;
  return acceptor1;
}


static inline _L4_word_t
_L4_attribute_always_inline
_L4_has_string_items (_L4_acceptor_t acceptor)
{
  __L4_acceptor_t _acceptor;

  _acceptor.raw = acceptor;
  return _acceptor.string_items;
}


static inline _L4_word_t
_L4_attribute_always_inline
_L4_has_map_grant_items (_L4_acceptor_t acceptor)
{
  __L4_acceptor_t _acceptor;

  _acceptor.raw = acceptor;
  return _acceptor.rcv_window;
}


static inline _L4_fpage_t
_L4_attribute_always_inline
_L4_rcv_window (_L4_acceptor_t acceptor)
{
  __L4_acceptor_t _acceptor;

  _acceptor.raw = acceptor;
  _acceptor.string_items = 0;
  return (_L4_fpage_t) _acceptor.rcv_window;
}


static inline void
_L4_attribute_always_inline
_L4_accept (_L4_acceptor_t acceptor)
{
  _L4_load_br (0, acceptor);
}


static inline void
_L4_attribute_always_inline
_L4_accept_strings (_L4_acceptor_t acceptor, _L4_msg_buffer_t msg_buffer)
{
  __L4_string_item_t *string_item = (__L4_string_item_t *) msg_buffer;
  int br = 1;
  int cont;

  _L4_load_br (0, acceptor);
  do
    {
      int nr = 1 + string_item->nr_substrings + 1;
      cont = string_item->cont || string_item->not_last;
      _L4_load_brs (br, nr, (_L4_word_t *) string_item);
      br += nr;
      string_item = (__L4_string_item_t *) (((_L4_word_t *) string_item) + nr);
    }
  while (cont);
}


static inline _L4_acceptor_t
_L4_attribute_always_inline
_L4_accepted (void)
{
  _L4_acceptor_t acceptor;
  _L4_store_br (0, &acceptor);
  return acceptor;
}


static inline void
_L4_attribute_always_inline
_L4_msg_buffer_clear (_L4_msg_buffer_t msg_buffer)
{
  msg_buffer[0] = 0;
}


static inline void
_L4_attribute_always_inline
_L4_msg_buffer_append_simple_rcv_string (_L4_msg_buffer_t msg_buffer,
					 _L4_string_item_t string_item)
{
  __L4_string_item_t *target = (__L4_string_item_t *) msg_buffer;
  __L4_string_item_t _string_item;
  int cont;

  _string_item.raw = string_item;

  do
    {
      int nr = 1 + target->nr_substrings + 1;
      cont = target->cont | target->not_last;
      if (!cont)
	target->not_last = 1;
      target = (__L4_string_item_t *) (((_L4_word_t *) target) + nr);
    }
  while (cont);

  _string_item.not_last = 0;
  _string_item.nr_substrings = 0;
  *target = _string_item;
}


static inline void
_L4_attribute_always_inline
_L4_msg_buffer_append_rcv_string (_L4_msg_buffer_t msg_buffer,
				  _L4_string_item_t *string_item)
{
  __L4_string_item_t *target = (__L4_string_item_t *) msg_buffer;
  __L4_string_item_t *_string_item = (__L4_string_item_t *) string_item;
  _L4_word_t *brp;
  int cont;

  /* Find the end of the message buffer.  */
  do
    {
      int nr = 1 + target->nr_substrings + 1;
      cont = target->cont | target->not_last;
      if (!cont)
	target->not_last = 1;
      target = (__L4_string_item_t *) (((_L4_word_t *) target) + nr);
    }
  while (cont);

  brp = (_L4_word_t *) target;

  /* Copy the source string.  */
  do
    {
      int nr = _string_item->nr_substrings;
      _L4_word_t *substrings = &_string_item->string[1];

      cont = _string_item->cont;
      *((__L4_string_item_t *) brp) = *_string_item;
      brp += 2;

      while (nr-- > 0)
	*(brp++) = *(substrings++);
      _string_item = (__L4_string_item_t *) substrings;
    }
  while (cont);
}


/* Message composition.  */
typedef _L4_word_t _L4_msg_t[_L4_NUM_MRS];
typedef union
{
  _L4_msg_t _msg;
  __L4_msg_tag_t tag;
} __L4_msg_t;


static inline void
_L4_attribute_always_inline
_L4_msg_put (_L4_msg_t msg, _L4_word_t label, int untyped_nr,
	     _L4_word_t *untyped, int typed_nr, void *any_typed)
{
  __L4_msg_t *_msg = (__L4_msg_t *) msg;
  _L4_word_t *mrs = msg;
  _L4_word_t *typed = (_L4_word_t *) any_typed;

  _msg->tag.untyped = untyped_nr;
  _msg->tag.typed = typed_nr;
  _msg->tag.propagated = 0;
  _msg->tag.redirected = 0;
  _msg->tag.cross_cpu = 0;
  _msg->tag.error = 0;
  _msg->tag.label = label;

  while (untyped_nr--)
    *(mrs++) = *(untyped++);

  while (typed_nr--)
    *(mrs++) = *(typed++);
}


static inline void
_L4_attribute_always_inline
_L4_msg_get (_L4_msg_t msg, _L4_word_t *untyped, void *any_typed)
{
  __L4_msg_t *_msg = (__L4_msg_t *) msg;
  int untyped_nr = _msg->tag.untyped;
  int typed_nr = _msg->tag.typed;
  _L4_word_t *mrs = msg;
  _L4_word_t *typed = (_L4_word_t *) any_typed;

  while (untyped_nr--)
    *(untyped++) = *(mrs++);

  while (typed_nr--)
    *(typed++) = *(mrs++);
}


static inline _L4_msg_tag_t
_L4_attribute_always_inline
_L4_msg_msg_tag (_L4_msg_t msg)
{
  __L4_msg_t *_msg = (__L4_msg_t *) msg;
  return _msg->tag.raw;
}


static inline void
_L4_attribute_always_inline
_L4_set_msg_msg_tag (_L4_msg_t msg, _L4_msg_tag_t tag)
{
  __L4_msg_t *_msg = (__L4_msg_t *) msg;
  _msg->tag.raw = tag;
}


static inline _L4_word_t
_L4_attribute_always_inline
_L4_msg_msg_label (_L4_msg_t msg)
{
  __L4_msg_t *_msg = (__L4_msg_t *) msg;
  return _msg->tag.label;
}


static inline void
_L4_attribute_always_inline
_L4_set_msg_msg_label (_L4_msg_t msg, _L4_word_t label)
{
  __L4_msg_t *_msg = (__L4_msg_t *) msg;
  _msg->tag.label = label;
}


static inline void
_L4_attribute_always_inline
_L4_msg_load (_L4_msg_t msg)
{
  __L4_msg_t *_msg = (__L4_msg_t *) msg;
  _L4_load_mrs (0, 1 + _msg->tag.untyped + _msg->tag.typed, msg);
}


static inline void
_L4_attribute_always_inline
_L4_msg_store (_L4_msg_tag_t tag, _L4_msg_t msg)
{
  __L4_msg_t *_msg = (__L4_msg_t *) msg;
  _msg->tag.raw = tag;
  _L4_store_mrs (1, _msg->tag.untyped + _msg->tag.typed, &msg[1]);
}


static inline void
_L4_attribute_always_inline
_L4_msg_clear (_L4_msg_t msg)
{
  __L4_msg_t *_msg = (__L4_msg_t *) msg;
  _msg->tag.raw = _L4_niltag;
}


static inline void
_L4_attribute_always_inline
_L4_msg_append_word (_L4_msg_t msg, _L4_word_t data)
{
  __L4_msg_t *_msg = (__L4_msg_t *) msg;
  _L4_word_t new_untyped_nr = ++_msg->tag.untyped;
  _L4_word_t typed_nr = _msg->tag.typed;

  if (typed_nr)
    {
      _L4_word_t *mrs = &msg[new_untyped_nr + typed_nr];
      while (typed_nr--)
	{
	  *(mrs) = *(mrs - 1);
	  mrs--;
	}
      *mrs = data;
    }
  else
    msg[new_untyped_nr] = data;
}


static inline void
_L4_attribute_always_inline
_L4_msg_append_map_item (_L4_msg_t msg, _L4_map_item_t map_item)
{
  __L4_msg_t *_msg = (__L4_msg_t *) msg;
  /* The "not last" bit is ignored for sending.  */
  int pos = 1 + _msg->tag.untyped + _msg->tag.typed;

  ((__L4_map_item_t *) &msg[pos])->raw = map_item;
  _msg->tag.typed += 2;
}


static inline void
_L4_attribute_always_inline
_L4_msg_append_grant_item (_L4_msg_t msg, _L4_grant_item_t grant_item)
{
  __L4_msg_t *_msg = (__L4_msg_t *) msg;
  /* The "not last" bit is ignored for sending.  */
  int pos = 1 + _msg->tag.untyped + _msg->tag.typed;

  ((__L4_grant_item_t *) &msg[pos])->raw = grant_item;
  _msg->tag.typed += 2;
}
    
  
static inline void
_L4_attribute_always_inline
_L4_msg_append_simple_string_item (_L4_msg_t msg,
				   _L4_string_item_t string_item)
{
  __L4_msg_t *_msg = (__L4_msg_t *) msg;
  __L4_string_item_t _string_item;
  /* The "not last" bit is ignored for sending.  */
  int pos = 1 + _msg->tag.untyped + _msg->tag.typed;

  _string_item.raw = string_item;
  _string_item.cont = 0;
  _string_item.nr_substrings = 0;

  ((__L4_string_item_t *) &msg[pos])->raw = string_item;
  _msg->tag.typed += 2;
}
    
  
static inline void
_L4_attribute_always_inline
_L4_msg_append_string_item (_L4_msg_t msg, _L4_string_item_t *string_item)
{
  __L4_msg_t *_msg = (__L4_msg_t *) msg;
  __L4_string_item_t *_string_item = (__L4_string_item_t *) string_item;
  /* The "not last" bit is ignored for sending.  */
  int pos = 1 + _msg->tag.untyped + _msg->tag.typed;
  int cont;

  do
    {
      int nr = _string_item->nr_substrings;
      _L4_word_t *substrings = &_string_item->string[1];

      cont = _string_item->cont;
      msg[pos++] = _string_item->mr[0];
      msg[pos++] = _string_item->mr[1];

      while (nr-- > 0)
	msg[pos++] = *(substrings++);
      _string_item = (__L4_string_item_t *) substrings;
    }
  while (cont);

  _msg->tag.typed += pos - 1 - _msg->tag.untyped;
}


static inline void
_L4_attribute_always_inline
_L4_msg_put_word (_L4_msg_t msg, _L4_word_t nr, _L4_word_t data)
{
  msg[1 + nr] = data;
}


static inline void
_L4_attribute_always_inline
_L4_msg_put_map_item (_L4_msg_t msg, _L4_word_t nr, _L4_map_item_t map_item)
{
  __L4_msg_t *_msg = (__L4_msg_t *) msg;
  _L4_word_t pos = 1 + _msg->tag.untyped + nr;

  ((__L4_map_item_t *) &msg[pos])->raw = map_item;
}


static inline void
_L4_attribute_always_inline
_L4_msg_put_grant_item (_L4_msg_t msg, _L4_word_t nr,
			_L4_grant_item_t grant_item)
{
  __L4_msg_t *_msg = (__L4_msg_t *) msg;
  _L4_word_t pos = 1 + _msg->tag.untyped + nr;

  ((__L4_grant_item_t *) &msg[pos])->raw = grant_item;
}


static inline void
_L4_attribute_always_inline
_L4_msg_put_simple_string_item (_L4_msg_t msg, _L4_word_t nr,
				_L4_string_item_t string_item)
{
  __L4_msg_t *_msg = (__L4_msg_t *) msg;
  __L4_string_item_t _string_item;
  _L4_word_t pos = 1 + _msg->tag.untyped + nr;

  _string_item.raw = string_item;
  _string_item.cont = 0;
  _string_item.nr_substrings = 0;

  ((__L4_string_item_t *) &msg[pos])->raw = string_item;
}


static inline void
_L4_attribute_always_inline
_L4_msg_put_string_item (_L4_msg_t msg, _L4_word_t nr,
			 _L4_string_item_t *string_item)
{
  __L4_msg_t *_msg = (__L4_msg_t *) msg;
  __L4_string_item_t *_string_item = (__L4_string_item_t *) string_item;
  _L4_word_t pos = 1 + _msg->tag.untyped + nr;
  int cont;

  do
    {
      int nr = _string_item->nr_substrings;
      _L4_word_t *substrings = &_string_item->string[1];

      cont = _string_item->cont;
      msg[pos++] = _string_item->mr[0];
      msg[pos++] = _string_item->mr[1];

      while (nr-- > 0)
	msg[pos++] = *(substrings++);
      _string_item = (__L4_string_item_t *) substrings;
    }
  while (cont);
}


static inline _L4_word_t
_L4_attribute_always_inline
_L4_msg_word (_L4_msg_t msg, _L4_word_t nr)
{
  return msg[1 + nr];
}


static inline void
_L4_attribute_always_inline
_L4_msg_get_word (_L4_msg_t msg, _L4_word_t nr, _L4_word_t *data)
{
  *data = _L4_msg_word (msg, nr);
}


static inline _L4_word_t
_L4_attribute_always_inline
_L4_msg_get_map_item (_L4_msg_t msg, _L4_word_t nr, _L4_map_item_t *map_item)
{
  __L4_msg_t *_msg = (__L4_msg_t *) msg;
  _L4_word_t pos = 1 + _msg->tag.untyped + nr;

  *map_item = ((__L4_map_item_t *) &msg[pos])->raw;
  return sizeof (_L4_map_item_t) / sizeof (_L4_word_t);
}


static inline _L4_word_t
_L4_attribute_always_inline
_L4_msg_get_grant_item (_L4_msg_t msg, _L4_word_t nr,
			_L4_grant_item_t *grant_item)
{
  __L4_msg_t *_msg = (__L4_msg_t *) msg;
  _L4_word_t pos = 1 + _msg->tag.untyped + nr;

  *grant_item = ((__L4_grant_item_t *) &msg[pos])->raw;
  return sizeof (_L4_grant_item_t) / sizeof (_L4_word_t);
}


static inline _L4_word_t
_L4_attribute_always_inline
_L4_msg_get_string_item (_L4_msg_t msg, _L4_word_t nr,
			 _L4_string_item_t *string_item)
{
  __L4_msg_t *_msg = (__L4_msg_t *) msg;
  __L4_string_item_t *_string_item = (__L4_string_item_t *) string_item;
  _L4_word_t pos = 1 + _msg->tag.untyped + nr;
  int cont;

  do
    {
      int nr = _string_item->nr_substrings;
      _L4_word_t *substrings = &_string_item->string[1];

      cont = _string_item->cont;
      *((__L4_string_item_t *) &msg[pos]) = *_string_item;
      pos += 2;

      while (nr-- > 0)
	*(substrings++) = msg[pos++];
      _string_item = (__L4_string_item_t *) substrings;
    }
  while (cont);

  return ((_L4_word_t *) _string_item) - ((_L4_word_t *) string_item);
}


/* l4_ipc convenience interface.  */

static inline _L4_word_t
_L4_attribute_always_inline
_L4_timeouts (_L4_time_t send_timeout, _L4_time_t receive_timeout)
{
  return (send_timeout << 16) | receive_timeout;
}


static inline _L4_msg_tag_t
_L4_attribute_always_inline
_L4_call_timeouts (_L4_thread_id_t dest, _L4_time_t send_timeout,
		   _L4_time_t receive_timeout)
{
  _L4_thread_id_t from;
  return _L4_ipc (dest, dest, _L4_timeouts (send_timeout, receive_timeout),
		  &from);
}


static inline _L4_msg_tag_t
_L4_attribute_always_inline
_L4_call (_L4_thread_id_t dest)
{
  return _L4_call_timeouts (dest, _L4_never, _L4_never);
}


static inline _L4_msg_tag_t
_L4_attribute_always_inline
_L4_send_timeout (_L4_thread_id_t dest, _L4_time_t send_timeout)
{
  _L4_thread_id_t dummy;
  return _L4_ipc (dest, _L4_nilthread,
		  _L4_timeouts (send_timeout, _L4_zero_time),
		  &dummy);
}


static inline _L4_msg_tag_t
_L4_attribute_always_inline
_L4_send (_L4_thread_id_t dest)
{
  return _L4_send_timeout (dest, _L4_never);
}


static inline _L4_msg_tag_t
_L4_attribute_always_inline
_L4_reply (_L4_thread_id_t dest)
{
  return _L4_send_timeout (dest, _L4_zero_time);
}


static inline _L4_msg_tag_t
_L4_attribute_always_inline
_L4_receive_timeout (_L4_thread_id_t from, _L4_time_t receive_timeout)
{
  _L4_thread_id_t dummy;
  return _L4_ipc (_L4_nilthread, from,
		  _L4_timeouts (_L4_zero_time, receive_timeout), &dummy);
}


static inline _L4_msg_tag_t
_L4_attribute_always_inline
_L4_receive (_L4_thread_id_t from)
{
  return _L4_receive_timeout (from, _L4_never);
}


static inline _L4_msg_tag_t
_L4_attribute_always_inline
_L4_wait_timeout (_L4_time_t receive_timeout, _L4_thread_id_t *from)
{
  return _L4_ipc (_L4_nilthread, _L4_anythread,
		  _L4_timeouts (_L4_zero_time, receive_timeout), from);
}


static inline _L4_msg_tag_t
_L4_attribute_always_inline
_L4_wait (_L4_thread_id_t *from)
{
  return _L4_wait_timeout (_L4_never, from);
}


static inline _L4_msg_tag_t
_L4_attribute_always_inline
_L4_reply_wait_timeout (_L4_thread_id_t dest, _L4_time_t receive_timeout,
			_L4_thread_id_t *from)
{
  return _L4_ipc (dest, _L4_anythread,
		  _L4_timeouts (_L4_time_period (0), receive_timeout), from);
}


static inline _L4_msg_tag_t
_L4_attribute_always_inline
_L4_reply_wait (_L4_thread_id_t dest, _L4_thread_id_t *from)
{
  return _L4_reply_wait_timeout (dest, _L4_never, from);
}


static inline void
_L4_attribute_always_inline
_L4_sleep (_L4_time_t time)
{
  _L4_set_msg_tag (_L4_receive_timeout (_L4_my_local_id (), time));
}


static inline _L4_msg_tag_t
_L4_attribute_always_inline
_L4_lcall (_L4_thread_id_t dest)
{
  _L4_thread_id_t dummy;
  return _L4_lipc (dest, dest, _L4_timeouts (_L4_never, _L4_never), &dummy);
}


static inline _L4_msg_tag_t
_L4_attribute_always_inline
_L4_lreply_wait (_L4_thread_id_t dest, _L4_thread_id_t *from)
{
  return _L4_lipc (dest, dest, _L4_timeouts (_L4_time_period (0), _L4_never),
		   from);
}


static inline _L4_word_t
_L4_attribute_always_inline
_L4_ipc_succeeded (_L4_msg_tag_t tag)
{
  __L4_msg_tag_t _tag;

  _tag.raw = tag;
  return !_tag.error;
}


static inline _L4_word_t
_L4_attribute_always_inline
_L4_ipc_failed (_L4_msg_tag_t tag)
{
  __L4_msg_tag_t _tag;

  _tag.raw = tag;
  return _tag.error;
}


static inline _L4_word_t
_L4_attribute_always_inline
_L4_ipc_propagated (_L4_msg_tag_t tag)
{
  __L4_msg_tag_t _tag;

  _tag.raw = tag;
  return _tag.propagated;
}


static inline _L4_word_t
_L4_attribute_always_inline
_L4_ipc_redirected (_L4_msg_tag_t tag)
{
  __L4_msg_tag_t _tag;

  _tag.raw = tag;
  return _tag.redirected;
}


static inline _L4_word_t
_L4_attribute_always_inline
_L4_ipc_xcpu (_L4_msg_tag_t tag)
{
  __L4_msg_tag_t _tag;

  _tag.raw = tag;
  return _tag.cross_cpu;
}


static inline void
_L4_attribute_always_inline
_L4_set_propagation (_L4_msg_tag_t *tag)
{
  __L4_msg_tag_t *_tag = (__L4_msg_tag_t *) tag;

  _tag->propagated = 1;
}


/* Now incorporate the public interfaces the user has selected.  */
#ifdef _L4_INTERFACE_L4
#include <l4/compat/ipc.h>
#endif
#ifdef _L4_INTERFACE_GNU
#include <l4/gnu/ipc.h>
#endif

#endif	/* l4/ipc.h */
