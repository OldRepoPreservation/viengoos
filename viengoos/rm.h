/* rm.h - Resource manager interface.
   Copyright (C) 2007 Free Software Foundation, Inc.
   Written by Neal H. Walfield <neal@gnu.org>.

   This file is part of the GNU Hurd.

   The GNU Hurd is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 3 of the
   License, or (at your option) any later version.

   The GNU Hurd is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see
   <http://www.gnu.org/licenses/>.  */

#ifndef RM_RM_H
#define RM_RM_H

#include <assert.h>
#include <l4.h>

#include <hurd/types.h>
#include <hurd/addr.h>
#include <hurd/addr-trans.h>
#include <hurd/startup.h>

#include <errno.h>

extern struct hurd_startup_data *__hurd_startup_data;

enum rm_method_id
  {
    RM_putchar = 100,

    RM_folio_alloc = 200,
    RM_folio_free,
    RM_folio_object_alloc,

    RM_cap_copy = 300,
    RM_cap_read,

    RM_object_slot_copy_out = 400,
    RM_object_slot_copy_in,
    RM_object_slot_read,
  };

static inline const char *
rm_method_id_string (enum rm_method_id id)
{
  switch (id)
    {
    case RM_putchar:
      return "putchar";
    case RM_folio_alloc:
      return "folio_alloc";
    case RM_folio_free:
      return "folio_free";
    case RM_folio_object_alloc:
      return "folio_object_alloc";
    case RM_cap_copy:
      return "cap_copy";
    case RM_cap_read:
      return "cap_read";
    case RM_object_slot_copy_out:
      return "object_slot_copy_out";
    case RM_object_slot_copy_in:
      return "object_slot_copy_in";
    case RM_object_slot_read:
      return "object_slot_read";
    default:
      return "unknown method id";
    }
}

/* Echo the character CHR on the manager console.  */
static inline void
__attribute__((always_inline))
rm_putchar (int chr)
{
  l4_msg_tag_t tag;

  l4_accept (L4_UNTYPED_WORDS_ACCEPTOR);

  tag = l4_niltag;
  l4_msg_tag_set_label (&tag, RM_putchar);
  l4_msg_tag_set_untyped_words (&tag, 1);
  l4_msg_tag_set_typed_words (&tag, 0);
  l4_set_msg_tag (tag);
  l4_load_mr (1, (l4_word_t) chr);
  /* XXX: We should send data to the log server.  */
  tag = l4_send (__hurd_startup_data->rm);
}

/* RPC template.  ID is the method name, ARGS is the list of arguments
   as normally passed to a function, LOADER is code to load the in
   parameters, and STORER is code to load the out parameters.  The
   code assumes that the first MR contains the error code and returns
   this as the function return value.  If the IPC fails, EHOSTDOWN is
   returned.  */
#define RPCX(id, args, loader, storer) \
  static inline error_t \
  __attribute__((always_inline)) \
  rm_##id args \
  { \
    l4_msg_tag_t tag; \
    l4_msg_t msg; \
    \
    l4_accept (L4_UNTYPED_WORDS_ACCEPTOR); \
    \
    l4_msg_clear (msg); \
    tag = l4_niltag; \
    l4_msg_tag_set_label (&tag, RM_##id); \
    l4_msg_set_msg_tag (msg, tag); \
    loader; \
    l4_msg_load (msg); \
    tag = l4_call (__hurd_startup_data->rm); \
    \
    if (l4_ipc_failed (tag)) \
      return EHOSTDOWN; \
    \
    l4_word_t err; \
    l4_store_mr (1, &err); \
    \
    int idx __attribute__ ((unused)); \
    idx = 2; \
    storer; \
    \
    return err; \
  }

/* Load the argument ARG, which is of type TYPE into MR IDX.  */
#define RPCLOAD(type, arg) \
  { \
    assert ((sizeof (arg) & (sizeof (l4_word_t) - 1)) == 0); \
    union \
      { \
        type arg_value_; \
        l4_word_t raw[sizeof (type) / sizeof (l4_word_t)]; \
      } arg_union_ = { (arg) }; \
    for (int i_ = 0; i_ < sizeof (type) / sizeof (l4_word_t); i_ ++) \
      l4_msg_append_word (msg, arg_union_.raw[i_]); \
  }

/* Store the contents of MR IDX+1 into *ARG, which is of type TYPE.
   NB: IDX is thus the return parameter number, not the message
   register number; MR0 contains the error code.  */
#define RPCSTORE(type, arg) \
  { \
    assert ((sizeof (*arg) & (sizeof (l4_word_t) - 1)) == 0); \
    union \
      { \
        type a__; \
        l4_word_t *raw; \
      } arg_union_ = { (arg) }; \
    for (int i_ = 0; i_ < sizeof (*arg) / sizeof (l4_word_t); i_ ++) \
      l4_store_mr (idx ++, &arg_union_.raw[i_]); \
  }

/* RPC with 2 in parameters and no out parameters.  */
#define RPC2(id, type1, arg1, type2, arg2) \
  RPCX(id, \
       (type1 arg1, type2 arg2), \
       {RPCLOAD(type1, arg1) \
        RPCLOAD(type2, arg2) \
       }, \
       {})

/* RPC with 3 in parameters and no out parameters.  */
#define RPC3(id, type1, arg1, \
             type2, arg2, \
             type3, arg3) \
  RPCX(id, \
       (type1 arg1, type2 arg2, type3 arg3), \
       {RPCLOAD(type1, arg1) \
        RPCLOAD(type2, arg2) \
        RPCLOAD(type3, arg3) \
       }, \
       {})

/* RPC with 4 in parameters and no out parameters.  */
#define RPC4(id, type1, arg1, \
             type2, arg2, \
             type3, arg3, \
             type4, arg4) \
  RPCX(id, \
       (type1 arg1, type2 arg2, type3 arg3, type4 arg4), \
       {RPCLOAD(type1, arg1) \
        RPCLOAD(type2, arg2) \
        RPCLOAD(type3, arg3) \
        RPCLOAD(type4, arg4) \
       }, \
       {})

/* RPC with 5 in parameters and no out parameters.  */
#define RPC5(id, type1, arg1, \
             type2, arg2, \
             type3, arg3, \
             type4, arg4, \
             type5, arg5) \
  RPCX(id, \
       (type1 arg1, type2 arg2, type3 arg3, type4 arg4, type5 arg5), \
       {RPCLOAD(type1, arg1) \
        RPCLOAD(type2, arg2) \
        RPCLOAD(type3, arg3) \
        RPCLOAD(type4, arg4) \
        RPCLOAD(type5, arg5) \
       }, \
       {})

/* RPC with 6 in parameters and no out parameters.  */
#define RPC6(id, type1, arg1, \
             type2, arg2, \
             type3, arg3, \
             type4, arg4, \
             type5, arg5, \
             type6, arg6) \
  RPCX(id, \
       (type1 arg1, type2 arg2, type3 arg3, type4 arg4, type5 arg5, \
        type6 arg6), \
       {RPCLOAD(type1, arg1) \
        RPCLOAD(type2, arg2) \
        RPCLOAD(type3, arg3) \
        RPCLOAD(type4, arg4) \
        RPCLOAD(type5, arg5) \
        RPCLOAD(type6, arg6) \
       }, \
       {})

/* RPC with 7 in parameters and no out parameters.  */
#define RPC7(id, type1, arg1, \
             type2, arg2, \
             type3, arg3, \
             type4, arg4, \
             type5, arg5, \
             type6, arg6, \
             type7, arg7) \
  RPCX(id, \
       (type1 arg1, type2 arg2, type3 arg3, type4 arg4, type5 arg5, \
        type6 arg6, type7 arg7), \
       {RPCLOAD(type1, arg1) \
        RPCLOAD(type2, arg2) \
        RPCLOAD(type3, arg3) \
        RPCLOAD(type4, arg4) \
        RPCLOAD(type5, arg5) \
        RPCLOAD(type6, arg6) \
        RPCLOAD(type7, arg7) \
       }, \
       {})

/* RPC with 8 in parameters and no out parameters.  */
#define RPC8(id, type1, arg1, \
             type2, arg2, \
             type3, arg3, \
             type4, arg4, \
             type5, arg5, \
             type6, arg6, \
             type7, arg7, \
             type8, arg8) \
  RPCX(id, \
       (type1 arg1, type2 arg2, type3 arg3, type4 arg4, type5 arg5, \
        type6 arg6, type7 arg7, type8 arg8), \
       {RPCLOAD(type1, arg1) \
        RPCLOAD(type2, arg2) \
        RPCLOAD(type3, arg3) \
        RPCLOAD(type4, arg4) \
        RPCLOAD(type5, arg5) \
        RPCLOAD(type6, arg6) \
        RPCLOAD(type7, arg7) \
        RPCLOAD(type8, arg8) \
       }, \
       {})

/* RPC with 9 in parameters and no out parameters.  */
#define RPC9(id, type1, arg1, \
             type2, arg2, \
             type3, arg3, \
             type4, arg4, \
             type5, arg5, \
             type6, arg6, \
             type7, arg7, \
             type8, arg8, \
             type9, arg9) \
  RPCX(id, \
       (type1 arg1, type2 arg2, type3 arg3, type4 arg4, type5 arg5, \
        type6 arg6, type7 arg7, type8 arg8, type9 arg9), \
       {RPCLOAD(type1, arg1) \
        RPCLOAD(type2, arg2) \
        RPCLOAD(type3, arg3) \
        RPCLOAD(type4, arg4) \
        RPCLOAD(type5, arg5) \
        RPCLOAD(type6, arg6) \
        RPCLOAD(type7, arg7) \
        RPCLOAD(type8, arg8) \
        RPCLOAD(type9, arg9) \
       }, \
       {})

/* RPC with 2 in parameters and 2 out parameters.  */
#define RPC22(id, type1, arg1, \
              type2, arg2, \
              otype1, oarg1, \
              otype2, oarg2) \
  RPCX(id, \
       (type1 arg1, type2 arg2, otype1 oarg1, otype2 oarg2), \
       {RPCLOAD(type1, arg1) \
        RPCLOAD(type2, arg2) \
       }, \
       { \
        RPCSTORE(otype1, oarg1) \
        RPCSTORE(otype2, oarg2) \
       })

/* RPC with 3 in parameters and 2 out parameters.  */
#define RPC32(id, type1, arg1, \
              type2, arg2, \
              type3, arg3, \
              otype1, oarg1, \
              otype2, oarg2) \
  RPCX(id, \
       (type1 arg1, type2 arg2, type3 arg3, otype1 oarg1, otype2 oarg2), \
       {l4_msg_tag_set_untyped_words (&tag, 3); \
        RPCLOAD(type1, arg1) \
        RPCLOAD(type2, arg2) \
        RPCLOAD(type3, arg3) \
       }, \
       { \
        RPCSTORE(otype1, oarg1) \
        RPCSTORE(otype2, oarg2) \
       })

/* RPC with 4 in parameters and 2 out parameters.  */
#define RPC42(id, type1, arg1, \
              type2, arg2, \
              type3, arg3, \
              type4, arg4, \
              otype1, oarg1, \
              otype2, oarg2) \
  RPCX(id, \
       (type1 arg1, type2 arg2, type3 arg3, type4 arg4, \
        otype1 oarg1, otype2 oarg2), \
       {RPCLOAD(type1, arg1) \
        RPCLOAD(type2, arg2) \
        RPCLOAD(type3, arg3) \
        RPCLOAD(type4, arg4) \
       }, \
       { \
        RPCSTORE(otype1, oarg1) \
        RPCSTORE(otype2, oarg2) \
       })

/* RPC with 5 in parameters and 2 out parameters.  */
#define RPC52(id, type1, arg1, \
              type2, arg2, \
              type3, arg3, \
              type4, arg4, \
              type5, arg5, \
              otype1, oarg1, \
              otype2, oarg2) \
  RPCX(id, \
       (type1 arg1, type2 arg2, type3 arg3, type4 arg4, type5 arg5, \
        otype1 oarg1, otype2 oarg2), \
       {RPCLOAD(type1, arg1) \
        RPCLOAD(type2, arg2) \
        RPCLOAD(type3, arg3) \
        RPCLOAD(type4, arg4) \
        RPCLOAD(type5, arg5) \
       }, \
       { \
        RPCSTORE(otype1, oarg1) \
        RPCSTORE(otype2, oarg2) \
       })

/* Allocate a folio against PRINCIPAL.  Store a capability in
   the caller's cspace in slot FOLIO.  */
RPC2(folio_alloc, addr_t, principal, addr_t, folio)
  
/* Free the folio designated by FOLIO.  PRINCIPAL pays.  */
RPC2(folio_free, addr_t, principal, addr_t, folio)

/* Allocate INDEXth object in folio FOLIO as an object of type TYPE.
   PRINCIPAL is charged.  If OBJECT_SLOT is not ADDR_VOID, then stores
   a capability to the allocated object in OBJECT_SLOT.  */
RPC5(folio_object_alloc, addr_t, principal,
     addr_t, folio, l4_word_t, index, l4_word_t, type, addr_t, object_slot)

enum
{
  /* Use subpage in CAP_ADDR_TRANS (must be a subset of subpage in
     SOURCE).  */
  CAP_COPY_COPY_SUBPAGE = 1 << 0,
  /* Use guard in TARGET, not the guard in CAP_ADDR_TRANS.  */
  CAP_COPY_COPY_GUARD = 1 << 1,
};

#define THREAD_ASPACE_SLOT 0
#define THREAD_ACTIVITY_SLOT 1

/* Copy capability SOURCE to the capability slot TARGET.
   ADDR_TRANS_FLAGS is a subset of CAP_COPY_GUARD, CAP_COPY_SUBPAGE,
   and CAP_COPY_PRESERVE_GUARD, bitwise-ored.  If CAP_COPY_GUARD is
   set, the guard descriptor in CAP_ADDR_TRANS is used, if
   CAP_COPY_PRESERVE_GUARD, the guard descriptor in TARGET, otherwise,
   the guard descriptor is copied from SOURCE.  If CAP_COPY_SUBPAGE is
   set, the subpage descriptor in CAP_ADDR_TRANS is used, otherwise,
   the subpage descriptor is copied from SOURCE.  */
RPC5(cap_copy, addr_t, principal, addr_t, target, addr_t, source,
     l4_word_t, addr_trans_flags, struct cap_addr_trans, cap_addr_trans)

/* Store the public bits of the capability CAP in *TYPE and
   *CAP_ADDR_TRANS.  */
RPC22(cap_read, addr_t, principal, addr_t, cap,
      l4_word_t *, type, struct cap_addr_trans *, cap_addr_trans)

/* Copy the capability from slot SLOT of the object OBJECT (relative
   to the start of the object's subpage) to slot TARGET.  */
RPC6(object_slot_copy_out, addr_t, principal,
     addr_t, object, l4_word_t, slot, addr_t, target,
     l4_word_t, flags, struct cap_addr_trans, cap_addr_trans)

/* Copy the capability from slot SOURCE to slot INDEX of the object
   OBJECT (relative to the start of the object's subpage).  */
RPC6(object_slot_copy_in, addr_t, principal,
     addr_t, object, l4_word_t, index, addr_t, source,
     l4_word_t, flags, struct cap_addr_trans, cap_addr_trans)

/* Store the public bits of the capability slot SLOT of object
   OBJECT in *TYPE and *CAP_ADDR.  */
RPC32(object_slot_read, addr_t, principal,
      addr_t, object, l4_word_t, slot,
      l4_word_t *, type, struct cap_addr_trans *, cap_addr_trans)

#endif
