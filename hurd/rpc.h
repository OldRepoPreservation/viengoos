/* rpc.h - RPC template definitions.
   Copyright (C) 2007 Free Software Foundation, Inc.
   Written by Neal H. Walfield <neal@gnu.org>.

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

#define RPC_CONCAT2(a,b) a##b
#define RPC_CONCAT(a,b) RPC_CONCAT2(a,b)

/* If RPC_STUB_PREFIX is defined, the prefix prepended plus an
   underscore to all function names.  If using, don't forget to #undef
   after all uses to avoid potential redefinition errors.  */
#undef RPC_STUB_PREFIX_
#ifndef RPC_STUB_PREFIX
#define RPC_STUB_PREFIX_(x) x
#else
#define RPC_STUB_PREFIX_(name) RPC_CONCAT(RPC_STUB_PREFIX,_##name)
#endif

/* If RPC_STUB_PREFIX is defined, the prefix prepended plus an
   underscore to all function names.  If using, don't forget to #undef
   after all uses to avoid potential redefinition errors.  */
#undef RPC_ID_PREFIX_
#ifndef RPC_ID_PREFIX
#define RPC_ID_PREFIX_(x) x
#else
#define RPC_ID_PREFIX_(name) RPC_CONCAT(RPC_ID_PREFIX,_##name)
#endif

/* We need to know where to send the IPC.  Either the caller can
   supply it or it can be implicit.

   If the caller should supply the target, then define
   RPC_TARGET_NEED_ARG, RPC_TARGET_ARG_TYPE to the type of the
   argument, and RPC_TARGET to a be a macro that takes a single
   argument and returns an l4_thread_id_t.

      #define RPC_STUB_PREFIX prefix
      #define RPC_ID_PREFIX PREFIX
      #define RPC_TARGET_NEED_ARG
      #define RPC_TARGET_ARG_TYPE object_t
      #define RPC_TARGET(x) ((x)->thread_id)

   If the caller need not supply the argument, then the includer
   should not define RPC_TARGET_NEED_ARG and should define RPC_TARGET
   to be a macro that takes no arguments and returns an
   l4_thread_id_t.

      #define RPC_STUB_PREFIX prefix
      #define RPC_ID_PREFIX PREFIX
      #undef RPC_TARGET_NEED_ARG
      #define RPC_TARGET ({ extern l4_thread_id_t foo_server; foo_server; })

    At the end of the include file, be sure to #undef the used
    preprocessor variables to avoid problems when multiple headers
    make use of this file.

      #undef RPC_STUB_PREFIX
      #undef RPC_ID_PREFIX
      #undef RPC_TARGET_NEED_ARG
      #undef RPC_TARGET_ARG_TYPE
      #undef RPC_TARGET
  */
#ifndef RPC_TARGET
#error Did not define RPC_TARGET
#endif

#undef RPC_TARGET_ARG_
#undef RPC_TARGET_

#ifdef RPC_TARGET_NEED_ARG
# ifndef RPC_TARGET_ARG_TYPE
#  error RPC_TARGET_NEED_ARG define but RPC_TARGET_ARG_TYPE not defined.
# endif

# define RPC_TARGET_ARG_ RPC_TARGET_ARG_TYPE arg_,
# define RPC_TARGET_ RPC_TARGET(arg_)
#else

# define RPC_TARGET_ARG_
# define RPC_TARGET_ RPC_TARGET
#endif

#undef RPC_TARGET_NEED_ARG

#ifndef _HURD_RPC_H
#define _HURD_RPC_H

#include <l4/ipc.h>
#include <errno.h>

/* Marshal the in-arguments and return a message buffer.  */
#define RPC_MARSHAL(id, idescs, loader) \
  static inline void \
  RPC_CONCAT (RPC_STUB_PREFIX_(id), _marshal) idescs \
  { \
    l4_msg_tag_t tag; \
    \
    tag = l4_niltag; \
    l4_msg_tag_set_label (&tag, RPC_ID_PREFIX_(id)); \
    \
    l4_msg_clear (*msg); \
    l4_msg_set_msg_tag (*msg, tag); \
    \
    loader; \
  }

/* Unmarshal the reply.  */
#define RPC_UNMARSHAL(id, odescs, storer) \
  static inline error_t \
  RPC_CONCAT (RPC_STUB_PREFIX_(id), _unmarshal) odescs \
  { \
    l4_msg_tag_t tag = l4_msg_msg_tag (*msg); \
    l4_word_t err = l4_msg_word (*msg, 0); \
    \
    int idx __attribute__ ((unused)); \
    idx = 1; \
    storer; \
    if (err == 0) \
      assert (idx == l4_untyped_words (tag)); \
    return err; \
  }

/* RPC template.  ID is the method name, ARGS is the list of arguments
   as normally passed to a function, LOADER is code to load the in
   parameters, and STORER is code to load the out parameters.  The
   code assumes that the first MR contains the error code and returns
   this as the function return value.  If the IPC fails, EHOSTDOWN is
   returned.  */
#define RPCX(id, args, idescs, iargs, loader, odescs, oargs, storer) \
  RPC_MARSHAL(id, idescs, loader) \
  RPC_UNMARSHAL(id, odescs, storer) \
  static inline error_t \
  __attribute__((always_inline)) \
  RPC_STUB_PREFIX_(id) args \
  { \
    l4_msg_tag_t tag; \
    l4_msg_t msg; \
    \
    RPC_CONCAT (RPC_STUB_PREFIX_(id), _marshal) iargs; \
    \
    l4_msg_load (msg); \
    l4_accept (L4_UNTYPED_WORDS_ACCEPTOR); \
    \
    tag = l4_call (RPC_TARGET_); \
    if (l4_ipc_failed (tag)) \
      return EHOSTDOWN; \
    \
    l4_msg_store (tag, msg); \
    return RPC_CONCAT (RPC_STUB_PREFIX_(id), _unmarshal) oargs; \
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
      l4_msg_append_word (*msg, arg_union_.raw[i_]); \
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
      arg_union_.raw[i_] = l4_msg_word (*msg, idx ++); \
  }

/* RPC with 2 in parameters and no out parameters.  */
#define RPC2(id, type1, arg1, type2, arg2) \
  RPCX(id, \
       (RPC_TARGET_ARG_ type1 arg1, type2 arg2), \
       (l4_msg_t *msg, type1 arg1, type2 arg2), \
       (&msg, arg1, arg2), \
       {RPCLOAD(type1, arg1) \
        RPCLOAD(type2, arg2) \
       }, \
       (l4_msg_t *msg), \
       (&msg), \
       {})

/* RPC with 3 in parameters and no out parameters.  */
#define RPC3(id, type1, arg1, \
             type2, arg2, \
             type3, arg3) \
  RPCX(id, \
       (RPC_TARGET_ARG_ type1 arg1, type2 arg2, type3 arg3), \
       (l4_msg_t *msg, type1 arg1, type2 arg2, type3 arg3), \
       (&msg, arg1, arg2, arg3), \
       {RPCLOAD(type1, arg1) \
        RPCLOAD(type2, arg2) \
        RPCLOAD(type3, arg3) \
       }, \
       (l4_msg_t *msg), \
       (&msg), \
       {})

/* RPC with 4 in parameters and no out parameters.  */
#define RPC4(id, type1, arg1, \
             type2, arg2, \
             type3, arg3, \
             type4, arg4) \
  RPCX(id, \
       (RPC_TARGET_ARG_ type1 arg1, type2 arg2, type3 arg3, type4 arg4), \
       (l4_msg_t *msg, type1 arg1, type2 arg2, type3 arg3, type4 arg4), \
       (&msg, arg1, arg2, arg3, arg4), \
       {RPCLOAD(type1, arg1) \
        RPCLOAD(type2, arg2) \
        RPCLOAD(type3, arg3) \
        RPCLOAD(type4, arg4) \
       }, \
       (l4_msg_t *msg), \
       (&msg), \
       {})

/* RPC with 5 in parameters and no out parameters.  */
#define RPC5(id, type1, arg1, \
             type2, arg2, \
             type3, arg3, \
             type4, arg4, \
             type5, arg5) \
  RPCX(id, \
       (RPC_TARGET_ARG_ type1 arg1, type2 arg2, type3 arg3, type4 arg4, \
        type5 arg5), \
       (l4_msg_t *msg, type1 arg1, type2 arg2, type3 arg3, \
        type4 arg4, type5 arg5), \
       (&msg, arg1, arg2, arg3, arg4, arg5), \
       {RPCLOAD(type1, arg1) \
        RPCLOAD(type2, arg2) \
        RPCLOAD(type3, arg3) \
        RPCLOAD(type4, arg4) \
        RPCLOAD(type5, arg5) \
       }, \
       (l4_msg_t *msg), \
       (&msg), \
       {})

/* RPC with 6 in parameters and no out parameters.  */
#define RPC6(id, type1, arg1, \
             type2, arg2, \
             type3, arg3, \
             type4, arg4, \
             type5, arg5, \
             type6, arg6) \
  RPCX(id, \
       (RPC_TARGET_ARG_ type1 arg1, type2 arg2, type3 arg3, type4 arg4, \
        type5 arg5, type6 arg6), \
       (l4_msg_t *msg, type1 arg1, type2 arg2, type3 arg3, type4 arg4, \
        type5 arg5, type6 arg6), \
       (&msg, arg1, arg2, arg3, arg4, arg5, arg6), \
       {RPCLOAD(type1, arg1) \
        RPCLOAD(type2, arg2) \
        RPCLOAD(type3, arg3) \
        RPCLOAD(type4, arg4) \
        RPCLOAD(type5, arg5) \
        RPCLOAD(type6, arg6) \
       }, \
       (l4_msg_t *msg), \
       (&msg), \
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
       (RPC_TARGET_ARG_ type1 arg1, type2 arg2, type3 arg3, type4 arg4, \
        type5 arg5, type6 arg6, type7 arg7), \
       (l4_msg_t *msg, type1 arg1, type2 arg2, type3 arg3, type4 arg4, \
        type5 arg5, type6 arg6, type7 arg7), \
       (&msg, arg1, arg2, arg3, arg4, arg5, arg6, arg7), \
       {RPCLOAD(type1, arg1) \
        RPCLOAD(type2, arg2) \
        RPCLOAD(type3, arg3) \
        RPCLOAD(type4, arg4) \
        RPCLOAD(type5, arg5) \
        RPCLOAD(type6, arg6) \
        RPCLOAD(type7, arg7) \
       }, \
       (l4_msg_t *msg), \
       (&msg), \
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
       (RPC_TARGET_ARG_ type1 arg1, type2 arg2, type3 arg3, type4 arg4, \
        type5 arg5, type6 arg6, type7 arg7, type8 arg8), \
       (l4_msg_t *msg, type1 arg1, type2 arg2, type3 arg3, type4 arg4, \
        type5 arg5, type6 arg6, type7 arg7, type8 arg8), \
       (&msg, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, msg), \
       {RPCLOAD(type1, arg1) \
        RPCLOAD(type2, arg2) \
        RPCLOAD(type3, arg3) \
        RPCLOAD(type4, arg4) \
        RPCLOAD(type5, arg5) \
        RPCLOAD(type6, arg6) \
        RPCLOAD(type7, arg7) \
        RPCLOAD(type8, arg8) \
       }, \
       (l4_msg_t *msg), \
       (&msg), \
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
       (RPC_TARGET_ARG_ type1 arg1, type2 arg2, type3 arg3, type4 arg4, \
        type5 arg5, type6 arg6, type7 arg7, type8 arg8, type9 arg9), \
       (l4_msg_t *msg, type1 arg1, type2 arg2, type3 arg3, type4 arg4, \
        type5 arg5, type6 arg6, type7 arg7, type8 arg8, type9 arg9, \
	l4_msg_t *msg), \
       (&msg, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9, msg), \
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
       (l4_msg_t *msg), \
       (&msg), \
       {})

/* RPC with 0 in parameters and 0 out parameters.  */
#define RPC00(id) \
  RPCX(id, \
       (RPC_TARGET_ARG_), \
       (l4_msg_t *msg), \
       (&msg), \
       {}, \
       (l4_msg_t *msg), \
       (&msg), \
       { \
       })

/* RPC with 0 in parameters and 3 out parameters.  */
#define RPC03(id, otype1, oarg1, \
              otype2, oarg2, \
              otype3, oarg3) \
  RPCX(id, \
       (RPC_TARGET_ARG_ otype1 oarg1, otype2 oarg2, otype3 oarg3), \
       (l4_msg_t *msg), \
       (&msg), \
       {}, \
       (l4_msg_t *msg, otype1 oarg1, otype2 oarg2, otype3 oarg3), \
       (&msg, oarg1, oarg2, oarg3), \
       { \
        RPCSTORE(otype1, oarg1) \
        RPCSTORE(otype2, oarg2) \
        RPCSTORE(otype3, oarg3) \
       })

/* RPC with 2 in parameters and 2 out parameters.  */
#define RPC22(id, type1, arg1, \
              type2, arg2, \
              otype1, oarg1, \
              otype2, oarg2) \
  RPCX(id, \
       (RPC_TARGET_ARG_ type1 arg1, type2 arg2, otype1 oarg1, otype2 oarg2), \
       (l4_msg_t *msg, type1 arg1, type2 arg2), \
       (&msg, arg1, arg2), \
       {RPCLOAD(type1, arg1) \
        RPCLOAD(type2, arg2) \
       }, \
       (l4_msg_t *msg, otype1 oarg1, otype2 oarg2), \
       (&msg, oarg1, oarg2), \
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
       (RPC_TARGET_ARG_ type1 arg1, type2 arg2, type3 arg3, \
        otype1 oarg1, otype2 oarg2), \
       (l4_msg_t *msg, type1 arg1, type2 arg2, type3 arg3), \
       (&msg, arg1, arg2, arg3), \
       {l4_msg_tag_set_untyped_words (&tag, 3); \
        RPCLOAD(type1, arg1) \
        RPCLOAD(type2, arg2) \
        RPCLOAD(type3, arg3) \
       }, \
       (l4_msg_t *msg, otype1 oarg1, otype2 oarg2), \
       (&msg, oarg1, oarg2), \
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
       (RPC_TARGET_ARG_ type1 arg1, type2 arg2, type3 arg3, type4 arg4, \
        otype1 oarg1, otype2 oarg2), \
       (l4_msg_t *msg, type1 arg1, type2 arg2, type3 arg3, type4 arg4), \
       (&msg, arg1, arg2, arg3, arg4), \
       {RPCLOAD(type1, arg1) \
        RPCLOAD(type2, arg2) \
        RPCLOAD(type3, arg3) \
        RPCLOAD(type4, arg4) \
       }, \
       (l4_msg_t *msg, otype1 oarg1, otype2 oarg2), \
       (&msg, oarg1, oarg2), \
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
       (RPC_TARGET_ARG_ type1 arg1, type2 arg2, type3 arg3, \
        type4 arg4, type5 arg5, \
        otype1 oarg1, otype2 oarg2), \
       (l4_msg_t *msg, type1 arg1, type2 arg2, type3 arg3, \
	type4 arg4, type5 arg5), \
       (&msg, arg1, arg2, arg3, arg4, arg5), \
       {RPCLOAD(type1, arg1) \
        RPCLOAD(type2, arg2) \
        RPCLOAD(type3, arg3) \
        RPCLOAD(type4, arg4) \
        RPCLOAD(type5, arg5) \
       }, \
       (l4_msg_t *msg, otype1 oarg1, otype2 oarg2), \
       (&msg, oarg1, oarg2), \
       { \
        RPCSTORE(otype1, oarg1) \
        RPCSTORE(otype2, oarg2) \
       })

/* RPC with 5 in parameters and 3 out parameters.  */
#define RPC53(id, type1, arg1, \
              type2, arg2, \
              type3, arg3, \
              type4, arg4, \
              type5, arg5, \
              otype1, oarg1, \
              otype2, oarg2, \
              otype3, oarg3) \
  RPCX(id, \
       (RPC_TARGET_ARG_ type1 arg1, type2 arg2, type3 arg3, \
        type4 arg4, type5 arg5, \
        otype1 oarg1, otype2 oarg2, otype3 oarg3), \
       (l4_msg_t *msg, type1 arg1, type2 arg2, type3 arg3, \
	type4 arg4, type5 arg5), \
       (&msg, arg1, arg2, arg3, arg4, arg5), \
       {RPCLOAD(type1, arg1) \
        RPCLOAD(type2, arg2) \
        RPCLOAD(type3, arg3) \
        RPCLOAD(type4, arg4) \
        RPCLOAD(type5, arg5) \
       }, \
       (l4_msg_t *msg, otype1 oarg1, otype2 oarg2, otype3 oarg3), \
       (&msg, oarg1, oarg2, oarg3),				  \
       { \
        RPCSTORE(otype1, oarg1) \
        RPCSTORE(otype2, oarg2) \
        RPCSTORE(otype3, oarg3) \
       })

/* RPC with 6 in parameters and 3 out parameters.  */
#define RPC63(id, type1, arg1, \
              type2, arg2, \
              type3, arg3, \
              type4, arg4, \
              type5, arg5, \
              type6, arg6, \
              otype1, oarg1, \
              otype2, oarg2, \
              otype3, oarg3) \
  RPCX(id, \
       (RPC_TARGET_ARG_ type1 arg1, type2 arg2, type3 arg3, \
        type4 arg4, type5 arg5, type6 arg6, \
        otype1 oarg1, otype2 oarg2, otype3 oarg3), \
       (l4_msg_t *msg, type1 arg1, type2 arg2, type3 arg3, \
	type4 arg4, type5 arg5, type6 arg6), \
       (&msg, arg1, arg2, arg3, arg4, arg5, arg6), \
       {RPCLOAD(type1, arg1) \
        RPCLOAD(type2, arg2) \
        RPCLOAD(type3, arg3) \
        RPCLOAD(type4, arg4) \
        RPCLOAD(type5, arg5) \
        RPCLOAD(type6, arg6) \
       }, \
       (l4_msg_t *msg, otype1 oarg1, otype2 oarg2, otype3 oarg3), \
       (&msg, oarg1, oarg2, oarg3),				  \
       { \
        RPCSTORE(otype1, oarg1) \
        RPCSTORE(otype2, oarg2) \
        RPCSTORE(otype3, oarg3) \
       })

/* RPC with 7 in parameters and 4 out parameters.  */
#define RPC74(id, type1, arg1, \
              type2, arg2, \
              type3, arg3, \
              type4, arg4, \
              type5, arg5, \
              type6, arg6, \
              type7, arg7, \
              otype1, oarg1, \
              otype2, oarg2, \
              otype3, oarg3, \
              otype4, oarg4) \
  RPCX(id, \
       (RPC_TARGET_ARG_ type1 arg1, type2 arg2, type3 arg3, \
        type4 arg4, type5 arg5, type6 arg6, type7 arg7, \
        otype1 oarg1, otype2 oarg2, otype3 oarg3, otype4 oarg4), \
       (l4_msg_t *msg, type1 arg1, type2 arg2, type3 arg3, \
	type4 arg4, type5 arg5, type6 arg6, type7 arg7), \
       (&msg, arg1, arg2, arg3, arg4, arg5, arg6, arg7), \
       {RPCLOAD(type1, arg1) \
        RPCLOAD(type2, arg2) \
        RPCLOAD(type3, arg3) \
        RPCLOAD(type4, arg4) \
        RPCLOAD(type5, arg5) \
        RPCLOAD(type6, arg6) \
        RPCLOAD(type7, arg7) \
       }, \
       (l4_msg_t *msg, otype1 oarg1, otype2 oarg2, otype3 oarg3, \
	otype4 oarg4), \
       (&msg, oarg1, oarg2, oarg3, oarg4),			  \
       { \
        RPCSTORE(otype1, oarg1) \
        RPCSTORE(otype2, oarg2) \
        RPCSTORE(otype3, oarg3) \
        RPCSTORE(otype4, oarg4) \
       })

/* RPC with 11 in parameters and 4 out parameters.  */
#define RPC11_4(id, type1, arg1, \
		type2, arg2,	 \
		type3, arg3,	 \
		type4, arg4,	 \
		type5, arg5,	 \
		type6, arg6,	 \
		type7, arg7,	 \
		type8, arg8,	 \
		type9, arg9,	 \
		type10, arg10,	 \
		type11, arg11,	 \
		otype1, oarg1,	 \
		otype2, oarg2,	 \
		otype3, oarg3,	 \
		otype4, oarg4)	 \
  RPCX(id, \
       (RPC_TARGET_ARG_ type1 arg1, type2 arg2, type3 arg3, \
        type4 arg4, type5 arg5, type6 arg6, type7 arg7, \
        type8 arg8, type9 arg9, type10 arg10, type11 arg11, \
        otype1 oarg1, otype2 oarg2, otype3 oarg3, otype4 oarg4), \
       (l4_msg_t *msg, type1 arg1, type2 arg2, type3 arg3, \
	type4 arg4, type5 arg5, type6 arg6, type7 arg7, \
	type8 arg8, type9 arg9, type10 arg10, type11 arg11), \
       (&msg, arg1, arg2, arg3, arg4, arg5, arg6, arg7, \
        arg8, arg9, arg10, arg11),    \
       {RPCLOAD(type1, arg1) \
        RPCLOAD(type2, arg2) \
        RPCLOAD(type3, arg3) \
        RPCLOAD(type4, arg4) \
        RPCLOAD(type5, arg5) \
        RPCLOAD(type6, arg6) \
        RPCLOAD(type7, arg7) \
        RPCLOAD(type8, arg8) \
        RPCLOAD(type9, arg9) \
        RPCLOAD(type10, arg10) \
        RPCLOAD(type11, arg11) \
       }, \
       (l4_msg_t *msg, otype1 oarg1, otype2 oarg2, otype3 oarg3, \
	otype4 oarg4), \
       (&msg, oarg1, oarg2, oarg3, oarg4),			  \
       { \
        RPCSTORE(otype1, oarg1) \
        RPCSTORE(otype2, oarg2) \
        RPCSTORE(otype3, oarg3) \
        RPCSTORE(otype4, oarg4) \
       })

/* RPC with 12 in parameters and 4 out parameters.  */
#define RPC12_4(id, type1, arg1, \
		type2, arg2,	 \
		type3, arg3,	 \
		type4, arg4,	 \
		type5, arg5,	 \
		type6, arg6,	 \
		type7, arg7,	 \
		type8, arg8,	 \
		type9, arg9,	 \
		type10, arg10,	 \
		type11, arg11,	 \
		type12, arg12,	 \
		otype1, oarg1,	 \
		otype2, oarg2,	 \
		otype3, oarg3,	 \
		otype4, oarg4)	 \
  RPCX(id, \
       (RPC_TARGET_ARG_ type1 arg1, type2 arg2, type3 arg3, \
        type4 arg4, type5 arg5, type6 arg6, type7 arg7, \
        type8 arg8, type9 arg9, type10 arg10, type11 arg11, type12 arg12, \
        otype1 oarg1, otype2 oarg2, otype3 oarg3, otype4 oarg4), \
       (l4_msg_t *msg, type1 arg1, type2 arg2, type3 arg3, \
	type4 arg4, type5 arg5, type6 arg6, type7 arg7, \
	type8 arg8, type9 arg9, type10 arg10, type11 arg11, type12 arg12), \
       (&msg, arg1, arg2, arg3, arg4, arg5, arg6, arg7, \
        arg8, arg9, arg10, arg11, arg12),    \
       {RPCLOAD(type1, arg1) \
        RPCLOAD(type2, arg2) \
        RPCLOAD(type3, arg3) \
        RPCLOAD(type4, arg4) \
        RPCLOAD(type5, arg5) \
        RPCLOAD(type6, arg6) \
        RPCLOAD(type7, arg7) \
        RPCLOAD(type8, arg8) \
        RPCLOAD(type9, arg9) \
        RPCLOAD(type10, arg10) \
        RPCLOAD(type11, arg11) \
        RPCLOAD(type12, arg12) \
       }, \
       (l4_msg_t *msg, otype1 oarg1, otype2 oarg2, otype3 oarg3, \
	otype4 oarg4), \
       (&msg, oarg1, oarg2, oarg3, oarg4),			  \
       { \
        RPCSTORE(otype1, oarg1) \
        RPCSTORE(otype2, oarg2) \
        RPCSTORE(otype3, oarg3) \
        RPCSTORE(otype4, oarg4) \
       })

/* RPC with 13 in parameters and 4 out parameters.  */
#define RPC13_4(id, type1, arg1, \
		type2, arg2,	 \
		type3, arg3,	 \
		type4, arg4,	 \
		type5, arg5,	 \
		type6, arg6,	 \
		type7, arg7,	 \
		type8, arg8,	 \
		type9, arg9,	 \
		type10, arg10,	 \
		type11, arg11,	 \
		type12, arg12,	 \
		type13, arg13,	 \
		otype1, oarg1,	 \
		otype2, oarg2,	 \
		otype3, oarg3,	 \
		otype4, oarg4)	 \
  RPCX(id, \
       (RPC_TARGET_ARG_ type1 arg1, type2 arg2, type3 arg3, \
        type4 arg4, type5 arg5, type6 arg6, type7 arg7, \
        type8 arg8, type9 arg9, type10 arg10, type11 arg11, type12 arg12, \
	type13 arg13, \
        otype1 oarg1, otype2 oarg2, otype3 oarg3, otype4 oarg4), \
       (l4_msg_t *msg, type1 arg1, type2 arg2, type3 arg3, \
	type4 arg4, type5 arg5, type6 arg6, type7 arg7, \
	type8 arg8, type9 arg9, type10 arg10, type11 arg11, type12 arg12, \
	type13 arg13), \
       (&msg, arg1, arg2, arg3, arg4, arg5, arg6, arg7, \
        arg8, arg9, arg10, arg11, arg12, arg13),    \
       {RPCLOAD(type1, arg1) \
        RPCLOAD(type2, arg2) \
        RPCLOAD(type3, arg3) \
        RPCLOAD(type4, arg4) \
        RPCLOAD(type5, arg5) \
        RPCLOAD(type6, arg6) \
        RPCLOAD(type7, arg7) \
        RPCLOAD(type8, arg8) \
        RPCLOAD(type9, arg9) \
        RPCLOAD(type10, arg10) \
        RPCLOAD(type11, arg11) \
        RPCLOAD(type12, arg12) \
        RPCLOAD(type13, arg13) \
       }, \
       (l4_msg_t *msg, otype1 oarg1, otype2 oarg2, otype3 oarg3, \
	otype4 oarg4), \
       (&msg, oarg1, oarg2, oarg3, oarg4),			  \
       { \
        RPCSTORE(otype1, oarg1) \
        RPCSTORE(otype2, oarg2) \
        RPCSTORE(otype3, oarg3) \
        RPCSTORE(otype4, oarg4) \
       })

#endif
