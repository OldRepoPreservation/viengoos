/* l4/ipc.h - Public interface to the L4 IPC primitive.
   Copyright (C) 2003, 2004, 2005 Free Software Foundation, Inc.
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
  __L4_msg_tag_t _tag = { .raw = tag };

  return _tag.label;
}


static inline _L4_word_t
_L4_attribute_always_inline
_L4_untyped_words (_L4_msg_tag_t tag)
{
  __L4_msg_tag_t _tag = { .raw = tag };

  return _tag.untyped;
}


static inline _L4_word_t
_L4_attribute_always_inline
_L4_typed_words (_L4_msg_tag_t tag)
{
  __L4_msg_tag_t _tag = { .raw = tag };

  return _tag.typed;
}


static inline _L4_msg_tag_t
_L4_attribute_always_inline
_L4_msg_tag_add_label (_L4_msg_tag_t tag, _L4_word_t label)
{
  __L4_msg_tag_t _tag = { .raw = tag };

  _tag.label = label;
  return _tag.raw;
}


static inline void
_L4_attribute_always_inline
_L4_msg_tag_add_label_to (_L4_msg_tag_t *tag, _L4_word_t label)
{
  __L4_msg_tag_t _tag = { .raw = *tag };

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


/* All items.  */

/* Get the continuation flag.  */
static inline _L4_word_t
_L4_attribute_always_inline
_L4_item_not_last (void *item)
{
  _L4_word_t _item;
  __builtin_memcpy (&_item, item, sizeof (_L4_word_t));

  /* The not_last flag is always the lowest bit in the item.  */
  return _item & 1;
}


/* Get the continuation flag.  */
static inline void
_L4_attribute_always_inline
_L4_set_item_not_last (void *item, _L4_word_t val)
{
  _L4_word_t _item;
  __builtin_memcpy (&_item, item, sizeof (_L4_word_t));

  /* The not_last flag is always the lowest bit in the item.  */
  _item = (_item & ~0) | !!val;

  __builtin_memcpy (item, &_item, sizeof (_L4_word_t));
}


/* Map items.  */

typedef _L4_dword_t _L4_map_item_t;
 
typedef union
{
  _L4_map_item_t raw;

  /* We need the following member to avoid breaking the aliasing rules.  */
  _L4_word_t mr[2];

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
  __L4_map_item_t _map_item = { .raw = map_item };

  return _map_item._four == 4;
}


static inline _L4_word_t
_L4_attribute_always_inline
_L4_map_item_snd_fpage (_L4_map_item_t map_item)
{
  __L4_map_item_t _map_item = { .raw = map_item };

  return _map_item.send_fpage;
}


static inline _L4_word_t
_L4_attribute_always_inline
_L4_map_item_snd_base (_L4_map_item_t map_item)
{
  __L4_map_item_t _map_item = { .raw = map_item };

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
  __L4_grant_item_t _grant_item = { .raw = grant_item };

  return _grant_item._five == 5;
}


static inline _L4_word_t
_L4_attribute_always_inline
_L4_grant_item_snd_fpage (_L4_grant_item_t grant_item)
{
  __L4_grant_item_t _grant_item = { .raw = grant_item };

  return _grant_item.send_fpage;
}


static inline _L4_word_t
_L4_attribute_always_inline
_L4_grant_item_snd_base (_L4_grant_item_t grant_item)
{
  __L4_grant_item_t _grant_item = { .raw = grant_item };

  return _grant_item.send_base << 10;
}


/* String items.  */

typedef _L4_dword_t _L4_string_item_t;

typedef union
{
  _L4_string_item_t raw;

  /* We need the following member to avoid breaking the aliasing rules.  */
  _L4_word_t mr[2];

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
_L4_is_string_item (void *string_item)
{
  __L4_string_item_t _string_item;
  __builtin_memcpy (&_string_item.raw, string_item, sizeof (_string_item.raw));

  return _string_item._zero == 0;
}


static inline _L4_word_t
_L4_attribute_always_inline
_L4_compound_string (void *string_item)
{
  __L4_string_item_t _string_item;
  __builtin_memcpy (&_string_item.raw, string_item, sizeof (_string_item.raw));

  return _string_item.cont;
}


static inline void
_L4_attribute_always_inline
_L4_set_compound_string (void *string_item, _L4_word_t cont)
{
  __L4_string_item_t _string_item;
  __builtin_memcpy (&_string_item.raw, string_item, sizeof (_string_item.raw));

  _string_item.cont = !!cont;

  __builtin_memcpy (string_item, &_string_item.raw, sizeof (_string_item.raw));
}


static inline _L4_word_t
_L4_attribute_always_inline
_L4_substrings (void *string_item)
{
  __L4_string_item_t _string_item;
  __builtin_memcpy (&_string_item.raw, string_item, sizeof (_string_item.raw));

  return _string_item.nr_substrings + 1;
}


static inline void
_L4_attribute_always_inline
_L4_set_substrings (void *string_item, _L4_word_t substrings)
{
  __L4_string_item_t _string_item;
  __builtin_memcpy (&_string_item.raw, string_item, sizeof (_string_item.raw));

  _string_item.nr_substrings = substrings - 1;

  __builtin_memcpy (string_item, &_string_item, sizeof (_string_item.raw));
}


static inline void *
_L4_attribute_always_inline
_L4_substring (void *string_item, _L4_word_t nr)
{
  /* This does not break strict aliasing, see below.  */
  __L4_string_item_t *_string_item = (__L4_string_item_t *) string_item;
  _L4_word_t string;

  /* To avoid breaking strict aliasing rules, we must not actually use
     _string_item for anything but pointer arithmetics, as we don't
     know the type of the underlying storage.  To be safe, we have to
     actually copy the memory through a char pointer.  */
  __builtin_memcpy (&string, &_string_item->string[nr - 1], sizeof (_L4_word_t));

  return (void *) string;
}


/* Count the number of all words used by the string item STRING_ITEM.
   If ALL_ITEMS is true, words used by following string items (until
   NOT_LAST is not set) will also be counted.  */
static inline _L4_word_t
_L4_attribute_always_inline
_L4_string_item_words (void *string_item, _L4_word_t all_items)
{
  /* We do not break any strict aliasing rules here, as we only use
     the pointers for pointer arithmetic.  */ 
  __L4_string_item_t *_string_item = (__L4_string_item_t *) string_item;
  int cont;
  int count = 0;

  do
    {
      int nr = 1 + _L4_substrings (_string_item);

      cont = _L4_compound_string (_string_item);
      if (all_items)
	cont = cont || _L4_item_not_last (_string_item);

      /* Remember how many words are used by all string items.  */
      count += nr;

      /* Skip over the current string item.  */
      _string_item = (__L4_string_item_t *)
	(((_L4_word_t *) _string_item) + nr);
    }
  while (cont);

  return count;
}


/* Append the string described by string item SOURCE to the string
   described by string item STRING_ITEM.  */
static inline void *
_L4_attribute_always_inline
_L4_add_substring_to (void *string_item, void *source)
{
  /* We do not break any strict aliasing rules here, as we only use
     the pointers for pointer arithmetic.  */ 
  __L4_string_item_t *target = (__L4_string_item_t *) string_item;
  __L4_string_item_t *_source = (__L4_string_item_t *) source;
  int cont;
  int count;

  /* First search for end of target string item.  */
  do
    {
      cont = _L4_compound_string (target);
      if (!cont)
	_L4_set_compound_string (target, 1);
      target = (__L4_string_item_t *)
	(((_L4_word_t *) target) + 1 + _L4_substrings (target));
    }
  while (cont);

  /* Now copy the source string item.  */
  count = _L4_string_item_words (_source, 0);
  while (count--)
    {
      __builtin_memcpy (target, _source, sizeof (_L4_word_t));
      target = (__L4_string_item_t *) (((_L4_word_t *) target) + 1);
      _source = (__L4_string_item_t *) (((_L4_word_t *) source) + 1);
    }

  return string_item;
}


/* Append the string pointed to by SOURCE as a substring (of the same
   length) to the string item STRING_ITEM.  */
static inline void *
_L4_attribute_always_inline
_L4_add_substring_address_to (void *string_item, void *source)
{
  /* We do not break any strict aliasing rules here, as we only use
     the pointers for pointer arithmetic.  */
  __L4_string_item_t *target = (__L4_string_item_t *) string_item;

  /* First search for end of target string item.  */
  while (_L4_compound_string (target))
    target = (__L4_string_item_t *)
      (((_L4_word_t *) target) + 1 + _L4_substrings (target));

  /* Now add the source substring.  */
  _L4_set_substrings (target, _L4_substrings (target) + 1);
  __builtin_memcpy (&target->string[_L4_substrings (target) - 1], source,
	  sizeof (_L4_word_t));

  return string_item;
}


static inline _L4_cache_allocation_hint_t
_L4_attribute_always_inline
_L4_cache_allocation_hint (_L4_string_item_t string_item)
{
  __L4_string_item_t _string_item = { .raw = string_item };

  return _string_item.cache_hint;
}


static inline _L4_string_item_t
_L4_attribute_always_inline
_L4_add_cache_allocation_hint (_L4_string_item_t string_item,
			       _L4_cache_allocation_hint_t hint)
{
  __L4_string_item_t _string_item = { .raw = string_item };

  _string_item.cache_hint = hint;
  return _string_item.raw;
}


static inline void *
_L4_attribute_always_inline
_L4_add_cache_allocation_hint_to (void *string_item,
				  _L4_cache_allocation_hint_t hint)
{ 
  __L4_string_item_t _string_item;
  __builtin_memcpy (&_string_item.raw, string_item, sizeof (_string_item.raw));

  _string_item.cache_hint = hint;

  __builtin_memcpy (string_item, &_string_item.raw, sizeof (_string_item.raw));
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


static inline _L4_fpage_t
_L4_attribute_always_inline
_L4_rcv_window (_L4_acceptor_t acceptor)
{
  __L4_acceptor_t _acceptor = { .raw = acceptor };

  /* The acceptor looks just like a receive window fpage, just that
     the lower bits have a different meaning.  So we just clear
     them.  */
  _acceptor.string_items = 0;

  return (_L4_fpage_t) _acceptor.raw;
}


static inline _L4_word_t
_L4_attribute_always_inline
_L4_has_map_grant_items (_L4_acceptor_t acceptor)
{
  /* FIXME: 0 is _L4_nilpage.  */
  return _L4_rcv_window (acceptor) != 0;
}


static inline _L4_acceptor_t
_L4_attribute_always_inline
_L4_map_grant_items (_L4_fpage_t rcv_window)
{
  __L4_acceptor_t acceptor = { .raw = rcv_window };

  acceptor.string_items = 0;
  acceptor._zero = 0;
  return acceptor.raw;
}


static inline _L4_acceptor_t
_L4_attribute_always_inline
_L4_add_acceptor (_L4_acceptor_t acceptor1, _L4_acceptor_t acceptor2)
{
  __L4_acceptor_t _acceptor1 = { .raw = acceptor1 };
  __L4_acceptor_t _acceptor2 = { .raw = acceptor2 };

  _acceptor1.string_items |= _acceptor2.string_items;
  if (_L4_has_map_grant_items (acceptor2))
    _acceptor1.rcv_window = _acceptor2.rcv_window;
  return _acceptor1.raw;
}


static inline _L4_acceptor_t *
_L4_attribute_always_inline
_L4_add_acceptor_to (_L4_acceptor_t *acceptor1, _L4_acceptor_t acceptor2)
{
  __L4_acceptor_t _acceptor1 = { .raw = *acceptor1 };
  __L4_acceptor_t _acceptor2 = { .raw = acceptor2 };

  _acceptor1.string_items |= _acceptor2.string_items;
  if (_L4_has_map_grant_items (acceptor2))
    _acceptor1.rcv_window = _acceptor2.rcv_window;

  *acceptor1 = _acceptor1.raw;
  return acceptor1;
}


static inline _L4_acceptor_t
_L4_attribute_always_inline
_L4_remove_acceptor (_L4_acceptor_t acceptor1, _L4_acceptor_t acceptor2)
{
  __L4_acceptor_t _acceptor1 = { .raw = acceptor1 };
  __L4_acceptor_t _acceptor2 = { .raw = acceptor2 };

  _acceptor1.string_items &= ~_acceptor2.string_items;
  if (_L4_has_map_grant_items (acceptor2))
    _acceptor1.rcv_window = 0;
  return _acceptor1.raw;
}


static inline _L4_acceptor_t *
_L4_attribute_always_inline
_L4_remove_acceptor_from (_L4_acceptor_t *acceptor1, _L4_acceptor_t acceptor2)
{
  __L4_acceptor_t _acceptor1 = { .raw = *acceptor1 };
  __L4_acceptor_t _acceptor2 = { .raw = acceptor2 };

  _acceptor1.string_items &= ~_acceptor2.string_items;
  if (_L4_has_map_grant_items (acceptor2))
    _acceptor1.rcv_window = 0;

  *acceptor1 = _acceptor1.raw;
  return acceptor1;
}


static inline _L4_word_t
_L4_attribute_always_inline
_L4_has_string_items (_L4_acceptor_t acceptor)
{
  __L4_acceptor_t _acceptor = { .raw = acceptor };

  return _acceptor.string_items;
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
  /* We do not break any strict aliasing rules here, as we only use
     the pointers for pointer arithmetic.  */
  __L4_string_item_t *string_item = (__L4_string_item_t *) msg_buffer;
  int count;

  _L4_load_br (0, acceptor);

  /* We do not check if the message buffer is empty.  If the user
     accepts string items, he better adds at least one string item
     buffer.  In any case, if he doesn't, we will only add a harmless
     simple string item of length 0 with a random address.  */
  count = _L4_string_item_words (string_item, 1);

  /* Load the string item buffers.  */
  _L4_load_brs (1, count, msg_buffer);
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

  /* The first word being not 0 is a sufficient, but not necessary
     indicator for a non-empty message buffer: A message with a simple
     string item of length 0 can also have a first word 0 (depending
     on the cache allocation hint).  The second word has only the
     pointer address, which is arbitrary, so it can't be used for
     differentiation either (0 is a valid pointer as far as L4 is
     concerned).

     So, we use the third word as an indicator if the message buffer
     is empty (in this case the first and the third word are 0), or if
     there is a single simple string item of length 0 in it (in this
     case the first word is 0, but the third word will be set to ~0 by
     _L4_msg_buffer_prepare_append).

     In all other cases (simple string item with length not 0,
     compound string items, more than one simple string item) the
     first word will not be 0.

     All this boils down to a simple rule: If the first and the third
     word are 0, then the message buffer is clear.  Otherwise, if at
     least one of them is not 0, then the message buffer contains some
     string items (you can determine how many by looking at the
     continuation flags and substring numbers).  */
  msg_buffer[2] = 0;
}


/* Search for the end of the message buffer, and set the not_last bit
   in the last existing item (thus preparing the message buffer for
   adding a new item).  */
static inline __L4_string_item_t *
_L4_attribute_always_inline
_L4_msg_buffer_prepare_append (_L4_msg_buffer_t msg_buffer)
{
  /* We do not break any strict aliasing rules here, as we only use
     the pointers for pointer arithmetic.  */
  __L4_string_item_t *target = (__L4_string_item_t *) msg_buffer;
  int found_last = 0;
  int cont;

  /* Check if this is an empty message buffer (see comment in
     _L4_msg_buffer_clear).  */
  if (msg_buffer[0] == 0 && msg_buffer[2] == 0)
    {
      /* It doesn't hurt to set the not-empty indicator here even if
	 we are not going to add a simple string item of length zero,
	 but something more complex.  */

      msg_buffer[2] = ~0;
      return target;
    }

  /* This is not an empty message buffer, so walk down the string
     items.  */
  do
    {
      int nr = 1 + _L4_substrings (target);
      cont = _L4_compound_string (target);

      /* Set the not_last bit in the last item.  */
      if (!_L4_item_not_last (target))
	{
	  _L4_set_item_not_last (target, 1);
	  found_last = 1;
	}

      /* Skip over this item.  */
      target = (__L4_string_item_t *) (((_L4_word_t *) target) + nr);
    }
  while (!found_last || cont);

  return target;
}


static inline void
_L4_attribute_always_inline
_L4_msg_buffer_append_simple_rcv_string (_L4_msg_buffer_t msg_buffer,
					 _L4_string_item_t string_item)
{
  __L4_string_item_t *target;
  __L4_string_item_t _string_item = { .raw = string_item };

  _string_item.not_last = 0;
  _string_item.nr_substrings = 0;

  target = _L4_msg_buffer_prepare_append (msg_buffer);
  __builtin_memcpy (target, &_string_item, sizeof (__L4_string_item_t));
}


static inline void
_L4_attribute_always_inline
_L4_msg_buffer_append_rcv_string (_L4_msg_buffer_t msg_buffer,
				  void *string_item)
{
  /* We do not break any strict aliasing rules here, as we only use
     the pointers for pointer arithmetic.  */
  __L4_string_item_t *target;
  __L4_string_item_t *_string_item = (__L4_string_item_t *) string_item;
  int cont;

  target = _L4_msg_buffer_prepare_append (msg_buffer);
  __builtin_memcpy (target, &_string_item, sizeof (__L4_string_item_t));

  _L4_set_item_not_last (_string_item, 0);

  /* Copy the source string.  */
  do
    {
      int nr = _L4_substrings (_string_item) + 1;
      cont = _L4_compound_string (_string_item);
      
      while (nr-- > 0)
	{
	  __builtin_memcpy (target, _string_item, sizeof (_L4_word_t));

	  target = (__L4_string_item_t *) (((_L4_word_t *) target) + 1);
	  _string_item = (__L4_string_item_t *)
	    (((_L4_word_t *) _string_item) + 1);
	}
    }
  while (cont);
}


/* Message composition.  */
typedef _L4_word_t _L4_msg_t[_L4_NUM_MRS];


static inline _L4_msg_tag_t
_L4_attribute_always_inline
_L4_msg_msg_tag (_L4_msg_t msg)
{
  return msg[0];
}


static inline void
_L4_attribute_always_inline
_L4_set_msg_msg_tag (_L4_msg_t msg, _L4_msg_tag_t tag)
{
  msg[0] = tag;
}


static inline void
_L4_attribute_always_inline
_L4_msg_put (_L4_msg_t msg, _L4_word_t label, int untyped_nr,
	     _L4_word_t *untyped, int typed_nr, void *any_typed)
{
  __L4_msg_tag_t tag;
  _L4_word_t *mrs = &msg[1];

  tag.untyped = untyped_nr;
  tag.typed = typed_nr;
  tag.propagated = 0;
  tag.redirected = 0;
  tag.cross_cpu = 0;
  tag.error = 0;
  tag.label = label;
  _L4_set_msg_msg_tag (msg, tag.raw);

  while (untyped_nr--)
    *(mrs++) = *(untyped++);

  /* We can't cast ANY_TYPED to anything useful and dereference it, as
     that would break strict aliasing rules.  This is why we use
     __builtin_memcpy, but only for known, small, fixed sizes, so that it is
     optimized away.  We know that sizeof (_L4_word_t) == sizeof (void *).  */
  while (typed_nr--)
    __builtin_memcpy (mrs++, any_typed++, sizeof (void *));
}


static inline void
_L4_attribute_always_inline
_L4_msg_get (_L4_msg_t msg, _L4_word_t *untyped, void *any_typed)
{
  __L4_msg_tag_t tag = { .raw = _L4_msg_msg_tag (msg) };
  int untyped_nr = tag.untyped;
  int typed_nr = tag.typed;
  _L4_word_t *mrs = &msg[1];

  while (untyped_nr--)
    *(untyped++) = *(mrs++);

  /* We can't cast ANY_TYPED to anything useful and dereference it, as
     that would break strict aliasing rules.  This is why we use
     __builtin_memcpy, but only for known, small, fixed sizes, so that it is
     optimized away.  We know that sizeof (_L4_word_t) == sizeof (void *).  */
  while (typed_nr--)
    __builtin_memcpy (any_typed++, mrs++, sizeof (_L4_word_t));
}


static inline _L4_word_t
_L4_attribute_always_inline
_L4_msg_msg_label (_L4_msg_t msg)
{
  __L4_msg_tag_t tag = { .raw = _L4_msg_msg_tag (msg) };
  return tag.label;
}


static inline void
_L4_attribute_always_inline
_L4_set_msg_msg_label (_L4_msg_t msg, _L4_word_t label)
{
  __L4_msg_tag_t tag = { .raw = _L4_msg_msg_tag (msg) };
  tag.label = label;
  _L4_set_msg_msg_tag (msg, tag.raw);
}


static inline void
_L4_attribute_always_inline
_L4_msg_load (_L4_msg_t msg)
{
  __L4_msg_tag_t tag = { .raw = _L4_msg_msg_tag (msg) };
  _L4_load_mrs (0, 1 + tag.untyped + tag.typed, msg);
}


static inline void
_L4_attribute_always_inline
_L4_msg_store (_L4_msg_tag_t tag, _L4_msg_t msg)
{
  __L4_msg_tag_t _tag = { .raw = tag };
  _L4_set_msg_msg_tag (msg, tag);
  _L4_store_mrs (1, _tag.untyped + _tag.typed, &msg[1]);
}


static inline void
_L4_attribute_always_inline
_L4_msg_clear (_L4_msg_t msg)
{
  _L4_set_msg_msg_tag (msg, _L4_niltag);
}


static inline void
_L4_attribute_always_inline
_L4_msg_append_word (_L4_msg_t msg, _L4_word_t data)
{
  __L4_msg_tag_t tag = { .raw = _L4_msg_msg_tag (msg) };
  _L4_word_t untyped_nr;
  _L4_word_t typed_nr;

  untyped_nr = tag.untyped;
  typed_nr = tag.typed;

  while (typed_nr)
    {
      __builtin_memcpy (&msg[untyped_nr + typed_nr + 1],
			&msg[untyped_nr + typed_nr],
			sizeof (_L4_word_t));
      typed_nr--;
    }

  tag.untyped++;
  msg[tag.untyped] = data;
  _L4_set_msg_msg_tag (msg, tag.raw);
}


static inline void
_L4_attribute_always_inline
_L4_msg_append_map_item (_L4_msg_t msg, _L4_map_item_t map_item)
{
  __L4_msg_tag_t tag = { .raw = _L4_msg_msg_tag (msg) };
  /* The "not last" bit is ignored for sending.  */
  int pos = 1 + tag.untyped + tag.typed;

  __builtin_memcpy (&msg[pos], &map_item, sizeof (_L4_map_item_t));
  tag.typed += sizeof (_L4_map_item_t) / sizeof (_L4_word_t);
  _L4_set_msg_msg_tag (msg, tag.raw);
}


static inline void
_L4_attribute_always_inline
_L4_msg_append_grant_item (_L4_msg_t msg, _L4_grant_item_t grant_item)
{
  __L4_msg_tag_t tag = { .raw = _L4_msg_msg_tag (msg) };
  /* The "not last" bit is ignored for sending.  */
  int pos = 1 + tag.untyped + tag.typed;

  __builtin_memcpy (&msg[pos], &grant_item, sizeof (_L4_grant_item_t));
  tag.typed += sizeof (_L4_grant_item_t) / sizeof (_L4_word_t);
  _L4_set_msg_msg_tag (msg, tag.raw);
}
    
  
static inline void
_L4_attribute_always_inline
_L4_msg_append_simple_string_item (_L4_msg_t msg,
				   _L4_string_item_t string_item)
{
  __L4_msg_tag_t tag = { .raw = _L4_msg_msg_tag (msg) };
  __L4_string_item_t _string_item = { .raw = string_item };
  /* The "not last" bit is ignored for sending.  */
  int pos = 1 + tag.untyped + tag.typed;

  _string_item.raw = string_item;
  _string_item.cont = 0;
  _string_item.nr_substrings = 0;

  __builtin_memcpy (&msg[pos], &_string_item, sizeof (__L4_string_item_t));
  tag.typed += sizeof (_L4_string_item_t) / sizeof (_L4_word_t);
  _L4_set_msg_msg_tag (msg, tag.raw);
}
    
  
static inline void
_L4_attribute_always_inline
_L4_msg_append_string_item (_L4_msg_t msg, void *string_item)
{
  __L4_msg_tag_t tag = { .raw = _L4_msg_msg_tag (msg) };
  /* We do not break any strict aliasing rules here, as we only use
     the pointers for pointer arithmetic.  */
  __L4_string_item_t *_string_item = (__L4_string_item_t *) string_item;
  /* The "not last" bit is ignored for sending.  */
  int pos = 1 + tag.untyped + tag.typed;
  int count;

  count = _L4_string_item_words (_string_item, 0);
  tag.typed += count;
  while (count--)
    {
      __builtin_memcpy (&msg[pos++], _string_item, sizeof (_L4_word_t));
      _string_item = (__L4_string_item_t *)
	(((_L4_word_t *) _string_item) + 1);
    }
  _L4_set_msg_msg_tag (msg, tag.raw);
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
  __L4_msg_tag_t tag = { .raw = _L4_msg_msg_tag (msg) };
  _L4_word_t pos = 1 + tag.untyped + nr;

  __builtin_memcpy (&msg[pos], &map_item, sizeof (_L4_map_item_t));
}


static inline void
_L4_attribute_always_inline
_L4_msg_put_grant_item (_L4_msg_t msg, _L4_word_t nr,
			_L4_grant_item_t grant_item)
{
  __L4_msg_tag_t tag = { .raw = _L4_msg_msg_tag (msg) };
  _L4_word_t pos = 1 + tag.untyped + nr;

  __builtin_memcpy (&msg[pos], &grant_item, sizeof (_L4_grant_item_t));
}


static inline void
_L4_attribute_always_inline
_L4_msg_put_simple_string_item (_L4_msg_t msg, _L4_word_t nr,
				_L4_string_item_t string_item)
{
  __L4_msg_tag_t tag = { .raw = _L4_msg_msg_tag (msg) };
  _L4_word_t pos = 1 + tag.untyped + nr;
  __L4_string_item_t _string_item = { .raw = string_item };
 
  _string_item.cont = 0;
  _string_item.nr_substrings = 0;

  __builtin_memcpy (&msg[pos], &_string_item, sizeof (__L4_string_item_t));
}


static inline void
_L4_attribute_always_inline
_L4_msg_put_string_item (_L4_msg_t msg, _L4_word_t nr, void *string_item)
{
  __L4_msg_tag_t tag = { .raw = _L4_msg_msg_tag (msg) };
  /* We do not break any strict aliasing rules here, as we only use
     the pointers for pointer arithmetic.  */
  __L4_string_item_t *_string_item = (__L4_string_item_t *) string_item;
  _L4_word_t pos = 1 + tag.untyped + nr;
  int count;

  count = _L4_string_item_words (_string_item, 0);
  while (count--)
    {
      __builtin_memcpy (&msg[pos++], _string_item, sizeof (_L4_word_t));
      _string_item = (__L4_string_item_t *)
	(((_L4_word_t *) _string_item) + 1);
    }
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
  __L4_msg_tag_t tag = { .raw = _L4_msg_msg_tag (msg) };
  _L4_word_t pos = 1 + tag.untyped + nr;

  __builtin_memcpy (map_item, &msg[pos], sizeof (_L4_map_item_t));
  return sizeof (_L4_map_item_t) / sizeof (_L4_word_t);
}


static inline _L4_word_t
_L4_attribute_always_inline
_L4_msg_get_grant_item (_L4_msg_t msg, _L4_word_t nr,
			_L4_grant_item_t *grant_item)
{
  __L4_msg_tag_t tag = { .raw = _L4_msg_msg_tag (msg) };
  _L4_word_t pos = 1 + tag.untyped + nr;

  __builtin_memcpy (grant_item, &msg[pos], sizeof (_L4_grant_item_t));
  return sizeof (_L4_grant_item_t) / sizeof (_L4_word_t);
}


static inline _L4_word_t
_L4_attribute_always_inline
_L4_msg_get_string_item (_L4_msg_t msg, _L4_word_t nr, void *string_item)
{
  __L4_msg_tag_t tag = { .raw = _L4_msg_msg_tag (msg) };
  /* We do not break any strict aliasing rules here, as we only use
     the pointers for pointer arithmetic.  */
  _L4_word_t pos = 1 + tag.untyped + nr;
  __L4_string_item_t *_string_item = (__L4_string_item_t *) &msg[pos];
  int count;
  int i;

  count = _L4_string_item_words (_string_item, 0);
  i = count;
  while (i--)
    {
      __builtin_memcpy (_string_item, &msg[pos++], sizeof (_L4_word_t));
      _string_item = (__L4_string_item_t *)
	(((_L4_word_t *) _string_item) + 1);
    }
  return count;
}


/* _L4_ipc convenience interface.  */

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
  __L4_msg_tag_t _tag = { .raw = tag };

  return !_tag.error;
}


static inline _L4_word_t
_L4_attribute_always_inline
_L4_ipc_failed (_L4_msg_tag_t tag)
{
  __L4_msg_tag_t _tag = { .raw = tag };

  return _tag.error;
}


static inline _L4_word_t
_L4_attribute_always_inline
_L4_ipc_propagated (_L4_msg_tag_t tag)
{
  __L4_msg_tag_t _tag = { .raw = tag };

  return _tag.propagated;
}


static inline _L4_word_t
_L4_attribute_always_inline
_L4_ipc_redirected (_L4_msg_tag_t tag)
{
  __L4_msg_tag_t _tag = { .raw = tag };

  return _tag.redirected;
}


static inline _L4_word_t
_L4_attribute_always_inline
_L4_ipc_xcpu (_L4_msg_tag_t tag)
{
  __L4_msg_tag_t _tag = { .raw = tag };

  return _tag.cross_cpu;
}


static inline void
_L4_attribute_always_inline
_L4_set_propagation (_L4_msg_tag_t *tag)
{
  __L4_msg_tag_t _tag = { .raw = *tag };

  _tag.propagated = 1;

  *tag = _tag.raw;
}


static inline void
_L4_attribute_always_inline
_L4_clear_propagation (_L4_msg_tag_t *tag)
{
  __L4_msg_tag_t _tag = { .raw = *tag };

  _tag.propagated = 0;

  *tag = _tag.raw;
}


/* Now incorporate the public interfaces the user has selected.  */
#ifdef _L4_INTERFACE_L4
#include <l4/compat/ipc.h>
#endif
#ifdef _L4_INTERFACE_GNU
#include <l4/gnu/ipc.h>
#endif

#endif	/* l4/ipc.h */
