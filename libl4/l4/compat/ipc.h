/* l4/compat/ipc.h - Public interface for L4 IPC.
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
# error "Never use <l4/compat/ipc.h> directly; include <l4/ipc.h> instead."
#endif


/* 5.1 Messages And Message Registers (MRs) [Virtual Registers]  */

/* Generic Programming Interface.  */
typedef struct
{
  L4_Word_t raw;
} L4_MsgTag_t;


#define	L4_Niltag	((L4_MsgTag_t) { .raw = _L4_niltag })

static inline L4_Bool_t
_L4_attribute_always_inline
#if defined(__cplusplus)
operator == (const L4_MsgTag_t& l, const L4_MsgTag_t& r)
#else
IsMsgTagEqual (L4_MsgTag_t l, L4_MsgTag_t r)
#endif
{
  return _L4_is_msg_tag_equal (l.raw, r.raw);
}


static inline L4_Bool_t
_L4_attribute_always_inline
#if defined(__cplusplus)
operator != (const L4_MsgTag_t& l, const L4_MsgTag_t& r)
#else
IsMsgTagNotEqual (L4_MsgTag_t l, L4_MsgTag_t r)
#endif
{
  return _L4_is_msg_tag_not_equal (l.raw, r.raw);
}


static inline L4_Word_t
_L4_attribute_always_inline
L4_Label (L4_MsgTag_t t)
{
  return _L4_label (t.raw);
}


static inline L4_Word_t
_L4_attribute_always_inline
L4_UntypedWords (L4_MsgTag_t t)
{
  return _L4_untyped_words (t.raw);
}


static inline L4_Word_t
_L4_attribute_always_inline
L4_TypedWords (L4_MsgTag_t t)
{
  return _L4_typed_words (t.raw);
}


static inline L4_MsgTag_t
_L4_attribute_always_inline
#if defined(__cplusplus)
operator + (const L4_MsgTag_t& l, const L4_Word_t& r)
#else
MsgTagAddLabel (const L4_MsgTag_t l, const L4_Word_t r)
#endif
{
  L4_MsgTag_t tag;

  tag.raw = _L4_msg_tag_add_label (l.raw, r);
  return tag;
}


static inline L4_MsgTag_t *
_L4_attribute_always_inline
#if defined(__cplusplus)
operator += (L4_MsgTag_t *l, const L4_Word_t& r)
#else
MsgTagAddLabelTo (L4_MsgTag_t *l, const L4_Word_t r)
#endif
{
  _L4_msg_tag_add_label_to (&l->raw, r);
  return l;
}


static inline L4_MsgTag_t
_L4_attribute_always_inline
L4_MsgTag (void)
{
  L4_MsgTag_t tag;

  tag.raw = _L4_msg_tag ();
  return tag;
}


static inline void
_L4_attribute_always_inline
L4_Set_MsgTag_t (L4_MsgTag_t tag)
{
  _L4_set_msg_tag (tag.raw);
}


/* Convenience Programming Interface.  */

typedef struct
{
  L4_Word_t raw[64];
} L4_Msg_t;


static inline void
_L4_attribute_always_inline
#if defined(__cplusplus)
L4_Put (L4_Msg_t *msg, L4_Word_t l, int u, L4_Word_t *ut, int t, void *items)
#else
L4_MsgPut (L4_Msg_t *msg, L4_Word_t l, int u, L4_Word_t *ut,
	   int t, void *items)
#endif
{
  _L4_msg_put (msg->raw, l, u, ut, t, items);
}


static inline void
_L4_attribute_always_inline
#if defined(__cplusplus)
L4_Get (L4_Msg_t *msg, L4_Word_t *ut, void *items)
#else
L4_MsgGet (L4_Msg_t *msg, L4_Word_t *ut, void *items)
#endif
{
  _L4_msg_get (msg->raw, ut, items);
}


static inline L4_MsgTag_t
_L4_attribute_always_inline
#if defined(__cplusplus)
L4_MsgTag (L4_Msg_t *msg)
#else
L4_MsgMsgTag (L4_Msg_t *msg)
#endif
{
  L4_MsgTag_t tag;

  tag.raw = _L4_msg_msg_tag (msg->raw);
  return tag;
}


static inline void
_L4_attribute_always_inline
#if defined(__cplusplus)
L4_Set_MsgTag (L4_Msg_t *msg, L4_MsgTag_t t)
#else
L4_Set_MsgMsgTag (L4_Msg_t *msg, L4_MsgTag_t t)
#endif
{
  _L4_set_msg_msg_tag (msg->raw, t.raw);
}


static inline L4_Word_t
_L4_attribute_always_inline
#if defined(__cplusplus)
L4_MsgLabel (L4_Msg_t *msg)
#else
L4_MsgMsgLabel (L4_Msg_t *msg)
#endif
{
  return _L4_msg_msg_label (msg->raw);
}


static inline void
_L4_attribute_always_inline
#if defined(__cplusplus)
L4_Set_MsgLabel (L4_Msg_t *msg, L4_Word_t label)
#else
L4_Set_MsgMsgLabel (L4_Msg_t *msg, L4_Word_t label)
#endif
{
  _L4_set_msg_msg_label (msg->raw, label);
}


static inline void
_L4_attribute_always_inline
#if defined(__cplusplus)
L4_Load (L4_Msg_t *msg)
#else
L4_MsgLoad (L4_Msg_t *msg)
#endif
{
  _L4_msg_load (msg->raw);
}


static inline void
_L4_attribute_always_inline
#if defined(__cplusplus)
L4_Store (L4_Msg_t *msg)
#else
L4_MsgStore (L4_MsgTag_t t, L4_Msg_t *msg)
#endif
{
  _L4_msg_store (t.raw, msg->raw);
}


static inline void
_L4_attribute_always_inline
#if defined(__cplusplus)
L4_Clear (L4_Msg_t *msg)
#else
L4_MsgClear (L4_Msg_t *msg)
#endif
{
  _L4_msg_clear (msg->raw);
}


/* The L4_Msg* interface is continued below, after defining the item
   types.  */


/* Low-Level MR Access is defined by <l4/message.h>.  */


/* 5.2 MapItem [Data type]  */

/* Generic Programming Interface.  */
typedef struct
{
  L4_Word_t raw[2];
} L4_MapItem_t;


static inline L4_MapItem_t
_L4_attribute_always_inline
L4_MapItem (L4_Fpage_t f, L4_Word_t SndBase)
{
  L4_MapItem_t m;

  *((_L4_map_item_t *) m.raw) = _L4_map_item (f.raw, SndBase);
  return m;
}


static inline L4_Bool_t
_L4_attribute_always_inline
#if defined(__cplusplus)
L4_MapItem (L4_MapItem_t m)
#else
L4_IsMapItem (L4_MapItem_t m)
#endif
{
  return _L4_is_map_item (*((_L4_map_item_t *) m.raw));
}


static inline L4_Fpage_t
_L4_attribute_always_inline
#if defined(__cplusplus)
L4_SndFpage (L4_MapItem_t m)
#else
L4_MapItemSndFpage (L4_MapItem_t m)
#endif
{
  L4_Fpage_t f;

  f.raw = _L4_map_item_snd_fpage (*((_L4_map_item_t *) m.raw));
  return f;
}


static inline L4_Word_t
_L4_attribute_always_inline
#if defined(__cplusplus)
L4_SndBase (L4_MapItem_t m)
#else
L4_MapItemSndBase (L4_MapItem_t m)
#endif
{
  return _L4_map_item_snd_base (*((_L4_map_item_t *) m.raw));
}


/* 5.3 GrantItem [Data type]  */

/* Generic Programming Interface.  */
typedef struct
{
  L4_Word_t raw[2];
} L4_GrantItem_t;


static inline L4_GrantItem_t
_L4_attribute_always_inline
L4_GrantItem (L4_Fpage_t f, L4_Word_t SndBase)
{
  L4_GrantItem_t g;

  *((_L4_grant_item_t *) g.raw) = _L4_grant_item (f.raw, SndBase);
  return g;
}


static inline L4_Bool_t
_L4_attribute_always_inline
#if defined(__cplusplus)
L4_GrantItem (L4_GrantItem_t g)
#else
L4_IsGrantItem (L4_GrantItem_t g)
#endif
{
  return _L4_is_grant_item (*((_L4_grant_item_t *) g.raw));
}


static inline L4_Fpage_t
_L4_attribute_always_inline
#if defined(__cplusplus)
L4_SndFpage (L4_GrantItem_t g)
#else
L4_GrantItemSndFpage (L4_GrantItem_t g)
#endif
{
  L4_Fpage_t f;

  f.raw = _L4_grant_item_snd_fpage (*((_L4_grant_item_t *) g.raw));
  return f;
}


static inline L4_Word_t
_L4_attribute_always_inline
#if defined(__cplusplus)
L4_SndBase (L4_GrantItem_t g)
#else
L4_GrantItemSndBase (L4_GrantItem_t g)
#endif
{
  return _L4_grant_item_snd_base (*((_L4_grant_item_t *) g.raw));
}


/* 5.4 StringItem [Data type]  */

/* Generic Programming Interface.  */
typedef struct
{
  L4_Word_t raw[2];
} L4_StringItem_t;


static inline L4_Bool_t
_L4_attribute_always_inline
#if defined(__cplusplus)
L4_StringItem (L4_StringItem_t *s)
#else
L4_IsStringItem (L4_StringItem_t *s)
#endif
{
  return _L4_is_string_item ((_L4_string_item_t *) s->raw);
}


static inline L4_Bool_t
_L4_attribute_always_inline
L4_CompoundString (L4_StringItem_t *s)
{
  return _L4_compound_string ((_L4_string_item_t *) s->raw);
}


static inline L4_Word_t
_L4_attribute_always_inline
L4_Substrings (L4_StringItem_t *s)
{
  return _L4_substrings ((_L4_string_item_t *) s->raw);
}


static inline void *
_L4_attribute_always_inline
L4_Substring (L4_StringItem_t *s, L4_Word_t n)
{
  return _L4_substring ((_L4_string_item_t *) s->raw, n);
}


static inline L4_StringItem_t
_L4_attribute_always_inline
L4_StringItem (int size, void *address)
{
  L4_StringItem_t s;

  *((_L4_string_item_t *) s.raw) = _L4_string_item (size, address);
  return s;
}


#if defined(__cplusplus)

/* FIXME: Add L4_StringItem_t& operator += (L4_StringItem_t& dest,
   L4_StringItem_t Addsub)?  */

static inline L4_StringItem_t&
_L4_attribute_always_inline
operator += (L4_StringItem_t& dest, L4_StringItem_t &substring)
{
  _L4_add_substring_to ((_L4_string_item_t *) dest.raw,
			(_L4_string_item_t *) substring.raw);
  return dest;
}


static inline L4_StringItem_t&
_L4_attribute_always_inline
operator += (L4_StringItem_t& dest, void *substringAddress)
{
  _L4_add_substring_address_to ((_L4_string_item_t *) dest.raw,
				substringAddress);
  return dest;
}

#else


static inline L4_StringItem_t *
_L4_attribute_always_inline
L4_AddSubstringTo (L4_StringItem_t *dest, L4_StringItem_t *substring)
{
  _L4_add_substring_to ((_L4_string_item_t *) dest->raw,
			(_L4_string_item_t *) substring->raw);
  return dest;
}


static inline L4_StringItem_t *
_L4_attribute_always_inline
L4_AddSubstringAddressTo (L4_StringItem_t *dest, void *substringAddress)
{
  _L4_add_substring_address_to ((_L4_string_item_t *) dest->raw,
				substringAddress);
  return dest;
}

#endif


/* Convenience Programming Interface.  */

typedef struct
{
  L4_Word_t raw;
} L4_CacheAllocationHint_t;


#define L4_UseDefaultCacheLineAllocation \
  ((L4_CacheAllocationHint_t) { .raw = _L4_use_default_cache_line_allocation })


static inline L4_Bool_t
_L4_attribute_always_inline
#if defined(__cplusplus)
operator == (const L4_CacheAllocationHint_t l, const L4_CacheAllocationHint_t r)
#else
L4_IsCacheAllocationHintEqual (const L4_CacheAllocationHint_t l,
			       const L4_CacheAllocationHint_t r)
#endif
{
  return l.raw == r.raw;
}


static inline L4_Bool_t
_L4_attribute_always_inline
#if defined(__cplusplus)
operator != (const L4_CacheAllocationHint_t l, const L4_CacheAllocationHint_t r)
#else
L4_IsCacheAllocationHintNotEqual (const L4_CacheAllocationHint_t l,
				  const L4_CacheAllocationHint_t r)
#endif
{
  return l.raw != r.raw;
}


static inline L4_CacheAllocationHint_t
_L4_attribute_always_inline
L4_CacheAllocationHint (L4_StringItem_t s)
{
  L4_CacheAllocationHint_t hint;

  hint.raw = _L4_cache_allocation_hint (*((_L4_string_item_t *) s.raw));
  return hint;
}


#if defined(__cplusplus)
static inline L4_StringItem_t
_L4_attribute_always_inline
operator + (const L4_StringItem_t& s, const L4_CacheAllocationHint hint)
{
  L4_StringItem_t _s = s;
  _L4_add_cache_allocation_hint_to ((_L4_string_item_t *) _s.raw, hint.raw);
  return _s;
}


static inline L4_StringItem_t&
_L4_attribute_always_inline
operator += (const L4_StringItem_t& s, const L4_CacheAllocationHint hint)
{
  _L4_add_cache_allocation_hint_to ((_L4_string_item_t *) s.raw, hint.raw);
  return s;
}

#else

static inline L4_StringItem_t
_L4_attribute_always_inline
L4_AddCacheAllocationHint (const L4_StringItem_t s,
			   const L4_CacheAllocationHint_t hint)
{
  L4_StringItem_t _s = s;
  _L4_add_cache_allocation_hint_to ((_L4_string_item_t *) _s.raw, hint.raw);
  return _s;
}



static inline L4_StringItem_t *
_L4_attribute_always_inline
L4_AddCacheAllocationHintTo (L4_StringItem_t *s,
			     const L4_CacheAllocationHint_t hint)
{
  _L4_add_cache_allocation_hint_to ((_L4_string_item_t *) s->raw, hint.raw);
  return s;
}

#endif


/* L4_Msg* interface continued.  */

static inline void
_L4_attribute_always_inline
#if defined(__cplusplus)
L4_Append (L4_Msg_t *msg, L4_Word_t w)
#else
L4_MsgAppendWord (L4_Msg_t *msg, L4_Word_t w)
#endif
{
  _L4_msg_append_word (msg->raw, w);
}


static inline void
_L4_attribute_always_inline
#if defined(__cplusplus)
L4_Append (L4_Msg_t *msg, L4_MapItem_t m)
#else
L4_MsgAppendMapItem (L4_Msg_t *msg, L4_MapItem_t m)
#endif
{
  _L4_msg_append_map_item (msg->raw, *((_L4_map_item_t *) m.raw));
}


static inline void
_L4_attribute_always_inline
#if defined(__cplusplus)
L4_Append (L4_Msg_t *msg, L4_GrantItem_t g)
#else
L4_MsgAppendGrantItem (L4_Msg_t *msg, L4_GrantItem_t g)
#endif
{
  _L4_msg_append_grant_item (msg->raw, *((_L4_grant_item_t *) g.raw));
}


static inline void
_L4_attribute_always_inline
#if defined(__cplusplus)
L4_Append (L4_Msg_t *msg, L4_StringItem_t s)
#else
L4_MsgAppendSimpleStringItem (L4_Msg_t *msg, L4_StringItem_t s)
#endif
{
  _L4_msg_append_simple_string_item (msg->raw, *((_L4_string_item_t *) s.raw));
}


static inline void
_L4_attribute_always_inline
#if defined(__cplusplus)
L4_Append (L4_Msg_t *msg, L4_StringItem_t *s)
#else
L4_MsgAppendStringItem (L4_Msg_t *msg, L4_StringItem_t *s)
#endif
{
  _L4_msg_append_string_item (msg->raw, (_L4_string_item_t *) s->raw);
}


static inline void
_L4_attribute_always_inline
#if defined(__cplusplus)
L4_Put (L4_Msg_t *msg, L4_Word_t u, L4_Word_t w)
#else
L4_MsgPutWord (L4_Msg_t *msg, L4_Word_t u, L4_Word_t w)
#endif
{
  _L4_msg_put_word (msg->raw, u, w);
}


static inline void
_L4_attribute_always_inline
#if defined(__cplusplus)
L4_Put (L4_Msg_t *msg, L4_Word_t t, L4_MapItem_t m)
#else
L4_MsgPutMapItem (L4_Msg_t *msg, L4_Word_t t, L4_MapItem_t m)
#endif
{
  _L4_msg_put_map_item (msg->raw, t, *((_L4_map_item_t *) m.raw));
}


static inline void
_L4_attribute_always_inline
#if defined(__cplusplus)
L4_Put (L4_Msg_t *msg, L4_Word_t t, L4_GrantItem_t g)
#else
L4_MsgPutGrantItem (L4_Msg_t *msg, L4_Word_t t, L4_GrantItem_t g)
#endif
{
  _L4_msg_put_grant_item (msg->raw, t, *((_L4_grant_item_t *) g.raw));
}


static inline void
_L4_attribute_always_inline
#if defined(__cplusplus)
L4_Put (L4_Msg_t *msg, L4_Word_t t, L4_StringItem_t s)
#else
L4_MsgPutSimpleStringItem (L4_Msg_t *msg, L4_Word_t t, L4_StringItem_t s)
#endif
{
  _L4_msg_put_simple_string_item (msg->raw, t, *((_L4_string_item_t *) s.raw));
}


static inline void
_L4_attribute_always_inline
#if defined(__cplusplus)
L4_Put (L4_Msg_t *msg, L4_Word_t t, L4_StringItem_t *s)
#else
L4_MsgPutStringItem (L4_Msg_t *msg, L4_Word_t t, L4_StringItem_t *s)
#endif
{
  _L4_msg_put_string_item (msg->raw, t, (_L4_string_item_t *) s->raw);
}


static inline L4_Word_t
_L4_attribute_always_inline
#if defined(__cplusplus)
L4_Get (L4_Msg_t *msg, L4_Word_t u)
#else
L4_MsgWord (L4_Msg_t *msg, L4_Word_t u)
#endif
{
  return _L4_msg_word (msg->raw, u);
}


static inline void
_L4_attribute_always_inline
#if defined(__cplusplus)
L4_Get (L4_Msg_t *msg, L4_Word_t u, L4_Word_t *w)
#else
L4_MsgGetWord (L4_Msg_t *msg, L4_Word_t u, L4_Word_t *w)
#endif
{
  _L4_msg_get_word (msg->raw, u, w);
}


static inline L4_Word_t
_L4_attribute_always_inline
#if defined(__cplusplus)
L4_Get (L4_Msg_t *msg, L4_Word_t t, L4_MapItem_t *m)
#else
L4_MsgGetMapItem (L4_Msg_t *msg, L4_Word_t t, L4_MapItem_t *m)
#endif
{
  return _L4_msg_get_map_item (msg->raw, t, (_L4_map_item_t *) m->raw);
}


static inline L4_Word_t
_L4_attribute_always_inline
#if defined(__cplusplus)
L4_Get (L4_Msg_t *msg, L4_Word_t t, L4_GrantItem_t *g)
#else
L4_MsgGetGrantItem (L4_Msg_t *msg, L4_Word_t t, L4_GrantItem_t *g)
#endif
{
  return _L4_msg_get_grant_item (msg->raw, t, (_L4_grant_item_t *) g->raw);
}


static inline L4_Word_t
_L4_attribute_always_inline
#if defined(__cplusplus)
L4_Get (L4_Msg_t *msg, L4_Word_t t, L4_StringItem_t *s)
#else
L4_MsgGetStringItem (L4_Msg_t *msg, L4_Word_t t, L4_StringItem_t *s)
#endif
{
  return _L4_msg_get_string_item (msg->raw, t, (_L4_string_item_t *) s->raw);
}


/* 5.5 String Buffers And Buffer Registers (BRs) [Pseudo Registers]  */

typedef struct
{
  L4_Word_t raw;
} L4_Acceptor_t;

#define L4_UntypedWordsAcceptor \
  ((L4_Acceptor_t) { .raw = _L4_untyped_words_acceptor })
#define L4_StringItemsAcceptor \
  ((L4_Acceptor_t) { .raw = _L4_string_items_acceptor })


static inline L4_Acceptor_t
_L4_attribute_always_inline
L4_MapGrantItem (L4_Fpage_t RcvWindow)
{
  L4_Acceptor_t a;

  a.raw = _L4_map_grant_items (RcvWindow.raw);
  return a;
}


static inline L4_Acceptor_t
_L4_attribute_always_inline
#if defined(__cplusplus)
operator + (const L4_Acceptor_t l, const L4_Acceptor_t r)
#else
L4_AddAcceptor (const L4_Acceptor_t l, const L4_Acceptor_t r)
#endif
{
  L4_Acceptor_t a;

  a.raw = _L4_add_acceptor (l.raw, r.raw);
  return a;
}


#if defined(__cplusplus)
static inline L4_Acceptor_t&
_L4_attribute_always_inline
operator += (L4_Acceptor_t& l, const L4_Acceptor_t r)
{
  l.raw = _L4_add_acceptor (l.raw, r.raw);
  return l;
}
#endif


static inline L4_Acceptor_t *
_L4_attribute_always_inline
L4_AddAcceptorTo (L4_Acceptor_t *l, const L4_Acceptor_t r)
{
  _L4_add_acceptor_to (&l->raw, r.raw);
  return l;
}


static inline L4_Acceptor_t
_L4_attribute_always_inline
#if defined(__cplusplus)
operator - (const L4_Acceptor_t l, const L4_Acceptor_t r)
#else
L4_RemoveAcceptor (const L4_Acceptor_t l, const L4_Acceptor_t r)
#endif
{
  L4_Acceptor_t a;

  a.raw = _L4_remove_acceptor (l.raw, r.raw);
  return a;
}


#if defined(__cplusplus)
static inline L4_Acceptor_t&
_L4_attribute_always_inline
operator += (L4_Acceptor_t& l, const L4_Acceptor_t r)
{
  l.raw = _L4_remove_acceptor (l.raw, r.raw);
  return l;
}
#endif


static inline L4_Acceptor_t *
_L4_attribute_always_inline
L4_RemoveAcceptorFrom (L4_Acceptor_t *l, const L4_Acceptor_t r)
{
  _L4_remove_acceptor_from (&l->raw, r.raw);
  return l;
}


static inline L4_Bool_t
_L4_attribute_always_inline
#if defined(__cplusplus)
L4_StringItems (L4_Acceptor_t a)
#else  
L4_HasStringItems (L4_Acceptor_t a)
#endif
{
  return _L4_has_string_items (a.raw);
}


static inline L4_Bool_t
_L4_attribute_always_inline
#if defined(__cplusplus)
L4_MapGrantItems (L4_Acceptor_t a)
#else  
L4_HasMapGrantItems (L4_Acceptor_t a)
#endif
{
  return _L4_has_map_grant_items (a.raw);
}


static inline L4_Fpage_t
_L4_attribute_always_inline
L4_RcvWindow (L4_Acceptor_t a)
{
  L4_Fpage_t f;
  f.raw = _L4_rcv_window (a.raw);
  return f;
}


static inline void
_L4_attribute_always_inline
L4_Accept (L4_Acceptor_t a)
{
  _L4_accept (a.raw);
}


/* L4_AcceptStrings is defined below.  */
static inline L4_Acceptor_t
_L4_attribute_always_inline
L4_Accepted (void)
{
  L4_Acceptor_t a;
  a.raw = _L4_accepted ();
  return a;
}


/* Convenience Programming Interface.  */

typedef struct
{
  L4_Word_t raw[32];
} L4_MsgBuffer_t;


static inline void
_L4_attribute_always_inline
#if defined(__cplusplus)
L4_Clear (L4_MsgBuffer_t *b)
#else
L4_MsgBufferClear (L4_MsgBuffer_t *b)
#endif
{
  _L4_msg_buffer_clear (b->raw);
}


static inline void
_L4_attribute_always_inline
#if defined(__cplusplus)
L4_Append (L4_MsgBuffer_t *b, L4_StringItem_t s)
#else
L4_MsgBufferAppendSimpleRcvString (L4_MsgBuffer_t *b, L4_StringItem_t s)
#endif
{
  _L4_msg_buffer_append_simple_rcv_string (b->raw,
					   *((_L4_string_item_t *) s.raw));
}


static inline void
_L4_attribute_always_inline
#if defined(__cplusplus)
L4_Append (L4_MsgBuffer_t *b, L4_StringItem_t *s)
#else
L4_MsgBufferAppendRcvString (L4_MsgBuffer_t *b, L4_StringItem_t *s)
#endif
{
  _L4_msg_buffer_append_rcv_string (b->raw, (_L4_string_item_t *) s->raw);
}


/* 5.5 continued.  */
static inline void
_L4_attribute_always_inline
#if defined(__cplusplus)
L4_Accept (L4_Acceptor_t a, L4_MsgBuffer_t *b)
#else
L4_AcceptStrings (L4_Acceptor_t a, L4_MsgBuffer_t *b)
#endif
{
  _L4_accept_strings (a.raw, b->raw);
}


/* Low-Level BR Access is defined by <l4/message.h>.  */


/* 5.6 Ipc [Systemcall]  */

/* Generic Programming Interface.  */

static inline L4_MsgTag_t
_L4_attribute_always_inline
L4_Ipc (L4_ThreadId_t to, L4_ThreadId_t FromSpecifier, L4_Word_t Timeouts,
	L4_ThreadId_t *from)
{
  L4_MsgTag_t t;

  t.raw = _L4_ipc (to.raw, FromSpecifier.raw, Timeouts, &from->raw);
  return t;
}


static inline L4_MsgTag_t
_L4_attribute_always_inline
L4_Lipc (L4_ThreadId_t to, L4_ThreadId_t FromSpecifier, L4_Word_t Timeouts,
	L4_ThreadId_t *from)
{
  L4_MsgTag_t t;

  t.raw = _L4_lipc (to.raw, FromSpecifier.raw, Timeouts, &from->raw);
  return t;
}


/* Convenience Programming Interface.  */

static inline L4_MsgTag_t
_L4_attribute_always_inline
L4_Call (L4_ThreadId_t to)
{
  L4_MsgTag_t t;

  t.raw = _L4_call (to.raw);
  return t;
}


static inline L4_MsgTag_t
_L4_attribute_always_inline
#if defined(__cplusplus)
L4_Call (L4_ThreadId_t to, L4_Time_t SndTimeout, L4_Time_t RcvTimeout)
#else
L4_Call_Timeouts (L4_ThreadId_t to, L4_Time_t SndTimeout, L4_Time_t RcvTimeout)
#endif
{
  L4_MsgTag_t t;

  t.raw = _L4_call_timeouts (to.raw, SndTimeout.raw, RcvTimeout.raw);
  return t;
}


static inline L4_MsgTag_t
_L4_attribute_always_inline
L4_Send (L4_ThreadId_t to)
{
  L4_MsgTag_t t;

  t.raw = _L4_send (to.raw);
  return t;
}


static inline L4_MsgTag_t
_L4_attribute_always_inline
#if defined(__cplusplus)
L4_Send (L4_ThreadId_t to, L4_Time_t SndTimeout)
#else
L4_Send_Timeout (L4_ThreadId_t to, L4_Time_t SndTimeout)
#endif
{
  L4_MsgTag_t t;

  t.raw = _L4_send_timeout (to.raw, SndTimeout.raw);
  return t;
}


static inline L4_MsgTag_t
_L4_attribute_always_inline
L4_Reply (L4_ThreadId_t to)
{
  L4_MsgTag_t t;

  t.raw = _L4_reply (to.raw);
  return t;
}


static inline L4_MsgTag_t
_L4_attribute_always_inline
L4_Receive (L4_ThreadId_t from)
{
  L4_MsgTag_t t;

  t.raw = _L4_receive (from.raw);
  return t;
}


static inline L4_MsgTag_t
_L4_attribute_always_inline
#if defined(__cplusplus)
L4_Receive (L4_ThreadId_t from, L4_Time_t RcvTimeout)
#else
L4_Receive_Timeout (L4_ThreadId_t from, L4_Time_t RcvTimeout)
#endif
{
  L4_MsgTag_t t;

  t.raw = _L4_receive_timeout (from.raw, RcvTimeout.raw);
  return t;
}


static inline L4_MsgTag_t
_L4_attribute_always_inline
L4_Wait (L4_ThreadId_t *from)
{
  L4_MsgTag_t t;

  t.raw = _L4_wait (&from->raw);
  return t;
}


static inline L4_MsgTag_t
_L4_attribute_always_inline
#if defined(__cplusplus)
L4_Wait (L4_Time_t RcvTimeout, L4_ThreadId_t *from)
#else
L4_Wait_Timeout (L4_Time_t RcvTimeout, L4_ThreadId_t *from)
#endif
{
  L4_MsgTag_t t;

  t.raw = _L4_wait_timeout (RcvTimeout.raw, &from->raw);
  return t;
}


static inline L4_MsgTag_t
_L4_attribute_always_inline
L4_ReplyWait (L4_ThreadId_t to, L4_ThreadId_t *from)
{
  L4_MsgTag_t t;

  t.raw = _L4_reply_wait (to.raw, &from->raw);
  return t;
}


static inline L4_MsgTag_t
_L4_attribute_always_inline
#if defined(__cplusplus)
L4_ReplyWait (L4_ThreadId_t to, L4_Time_t RcvTimeout, L4_ThreadId_t *from)
#else
L4_ReplyWait_Timeout (L4_ThreadId_t to, L4_Time_t RcvTimeout,
		      L4_ThreadId_t *from)
#endif
{
  L4_MsgTag_t t;

  t.raw = _L4_reply_wait_timeout (to.raw, RcvTimeout.raw, &from->raw);
  return t;
}


static inline void
_L4_attribute_always_inline
L4_Sleep (L4_Time_t t)
{
  _L4_sleep (t.raw);
}


static inline L4_MsgTag_t
_L4_attribute_always_inline
L4_Lcall (L4_ThreadId_t to)
{
  L4_MsgTag_t t;

  t.raw = _L4_lcall (to.raw);
  return t;
}


static inline L4_MsgTag_t
_L4_attribute_always_inline
L4_LreplyWait (L4_ThreadId_t to, L4_ThreadId_t *from)
{
  L4_MsgTag_t t;

  t.raw = _L4_lreply_wait (to.raw, &from->raw);
  return t;
}


/* Support Functions.  */

static inline L4_Bool_t
_L4_attribute_always_inline
L4_IpcSucceeded (L4_MsgTag_t t)
{
  return _L4_ipc_succeeded (t.raw);
}


static inline L4_Bool_t
_L4_attribute_always_inline
L4_IpcFailed (L4_MsgTag_t t)
{
  return _L4_ipc_failed (t.raw);
}


static inline L4_Bool_t
_L4_attribute_always_inline
L4_IpcPropagated (L4_MsgTag_t t)
{
  return _L4_ipc_propagated (t.raw);
}


static inline L4_Bool_t
_L4_attribute_always_inline
L4_IpcRedirected (L4_MsgTag_t t)
{
  return _L4_ipc_redirected (t.raw);
}


static inline L4_Bool_t
_L4_attribute_always_inline
L4_IpcXcpu (L4_MsgTag_t t)
{
  return _L4_ipc_xcpu (t.raw);
}


/* L4_ErrorCode(), L4_IntendedReceiver() and L4_ActualSender() are
   defined in <l4/compat/thread.h>.  */


static inline void
_L4_attribute_always_inline
L4_SetPropagation (L4_MsgTag_t *t)
{
  return _L4_set_propagation (&t->raw);
}


/* L4_SetVirtualSender() is defined in <l4/compat/thread.h>.  */


static inline L4_Word_t
_L4_attribute_always_inline
L4_Timeouts (L4_Time_t SndTimeout, L4_Time_t RcvTimeout)
{
  return _L4_timeouts (SndTimeout.raw, RcvTimeout.raw);
}
