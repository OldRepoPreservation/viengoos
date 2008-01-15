/* rpc.h - RPC template definitions.
   Copyright (C) 2007, 2008 Free Software Foundation, Inc.
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

#include <hurd/stddef.h>
#include <l4/ipc.h>
#include <errno.h>

/* First we define some cpp help macros.  */
#define CPP_IFTHEN_0(when, whennot) whennot
#define CPP_IFTHEN_1(when, whennot) when
#define CPP_IFTHEN_2(when, whennot) when
#define CPP_IFTHEN_3(when, whennot) when
#define CPP_IFTHEN_4(when, whennot) when
#define CPP_IFTHEN_5(when, whennot) when
#define CPP_IFTHEN_6(when, whennot) when
#define CPP_IFTHEN_7(when, whennot) when
#define CPP_IFTHEN_8(when, whennot) when
#define CPP_IFTHEN_9(when, whennot) when
#define CPP_IFTHEN_10(when, whennot) when
#define CPP_IFTHEN_11(when, whennot) when
#define CPP_IFTHEN_12(when, whennot) when
#define CPP_IFTHEN_13(when, whennot) when
#define CPP_IFTHEN_14(when, whennot) when
#define CPP_IFTHEN_15(when, whennot) when
#define CPP_IFTHEN_16(when, whennot) when
#define CPP_IFTHEN_17(when, whennot) when
#define CPP_IFTHEN_18(when, whennot) when
#define CPP_IFTHEN_19(when, whennot) when
#define CPP_IFTHEN_20(when, whennot) when
#define CPP_IFTHEN_21(when, whennot) when
#define CPP_IFTHEN_22(when, whennot) when
#define CPP_IFTHEN_23(when, whennot) when
#define CPP_IFTHEN_24(when, whennot) when
#define CPP_IFTHEN_25(when, whennot) when

#define CPP_IFTHEN_(expr, when, whennot)			\
  CPP_IFTHEN_##expr(when, whennot)
#define CPP_IFTHEN(expr, when, whennot)			\
  CPP_IFTHEN_(expr, when, whennot)
#define CPP_IF(expr, when)	\
  CPP_IFTHEN(expr, when,)
#define CPP_IFNOT(expr, whennot) \
  CPP_IFTHEN(expr, , whennot)

#define CPP_SUCC_0 1
#define CPP_SUCC_1 2
#define CPP_SUCC_2 3
#define CPP_SUCC_3 4
#define CPP_SUCC_4 5
#define CPP_SUCC_5 6
#define CPP_SUCC_6 7
#define CPP_SUCC_7 8
#define CPP_SUCC_8 9
#define CPP_SUCC_9 10
#define CPP_SUCC_10 11
#define CPP_SUCC_11 12
#define CPP_SUCC_12 13
#define CPP_SUCC_13 14
#define CPP_SUCC_14 15
#define CPP_SUCC_15 16
#define CPP_SUCC_16 17
#define CPP_SUCC_17 18
#define CPP_SUCC_18 19
#define CPP_SUCC_19 20
#define CPP_SUCC_20 21
#define CPP_SUCC_21 22
#define CPP_SUCC_22 23
#define CPP_SUCC_23 24
#define CPP_SUCC_24 25
#define CPP_SUCC_25 26

#define CPP_SUCC_(x) CPP_SUCC_##x
#define CPP_SUCC(x) CPP_SUCC_(x)

/* We'd like to define ADD as:

    #define ADD(x, y) \
      CPP_IFTHEN(y, ADD(SUCC(x), SUCC(y)), y)

  This does not work as while a macro is being expanded, it becomes
  ineligible for expansion.  Thus, any references (including indirect
  references) are not expanded.  Nested applications of a macro are,
  however, allowed, and this is what the CPP_APPLY macro does.  */
#define CPP_APPLY1(x, y) x(y)
#define CPP_APPLY2(x, y) x(CPP_APPLY1(x, y))
#define CPP_APPLY3(x, y) x(CPP_APPLY2(x, y))
#define CPP_APPLY4(x, y) x(CPP_APPLY3(x, y))
#define CPP_APPLY5(x, y) x(CPP_APPLY4(x, y))
#define CPP_APPLY6(x, y) x(CPP_APPLY5(x, y))
#define CPP_APPLY7(x, y) x(CPP_APPLY6(x, y))
#define CPP_APPLY8(x, y) x(CPP_APPLY7(x, y))
#define CPP_APPLY9(x, y) x(CPP_APPLY8(x, y))
#define CPP_APPLY10(x, y) x(CPP_APPLY9(x, y))
#define CPP_APPLY11(x, y) x(CPP_APPLY10(x, y))
#define CPP_APPLY12(x, y) x(CPP_APPLY11(x, y))
#define CPP_APPLY13(x, y) x(CPP_APPLY12(x, y))
#define CPP_APPLY14(x, y) x(CPP_APPLY13(x, y))
#define CPP_APPLY15(x, y) x(CPP_APPLY14(x, y))
#define CPP_APPLY16(x, y) x(CPP_APPLY15(x, y))
#define CPP_APPLY17(x, y) x(CPP_APPLY16(x, y))
#define CPP_APPLY18(x, y) x(CPP_APPLY17(x, y))
#define CPP_APPLY19(x, y) x(CPP_APPLY18(x, y))
#define CPP_APPLY20(x, y) x(CPP_APPLY19(x, y))
#define CPP_APPLY21(x, y) x(CPP_APPLY20(x, y))
#define CPP_APPLY22(x, y) x(CPP_APPLY21(x, y))
#define CPP_APPLY23(x, y) x(CPP_APPLY22(x, y))
#define CPP_APPLY24(x, y) x(CPP_APPLY23(x, y))
#define CPP_APPLY25(x, y) x(CPP_APPLY24(x, y))

#define ADD(x, y)				\
  CPP_IFTHEN(y, CPP_APPLY##y(CPP_SUCC, x), x)

/* CPP treats commas specially so we have to be smart about how we
   insert them algorithmically.  For instance, this won't work:

   #define COMMA ,
     CPP_IFTHEN(x, COMMA, )

   To optional insert a comma, use this function instead.  When the
   result is need, invoke the result.  For instance:

   RPC_IF_COMMA(x) ()
 */
#define RPC_COMMA() ,
#define RPC_NOCOMMA()
#define RPC_IF_COMMA(x) CPP_IFTHEN(x, RPC_COMMA, RPC_NOCOMMA)

/* Load the argument ARG, which is of type TYPE into MR IDX.  */
#define RPCLOADARG(__rla_type, __rla_arg)				\
  {									\
    union								\
    {									\
      __rla_type __rla_a;						\
      l4_word_t __rla_raw[(sizeof (__rla_type) + sizeof (l4_word_t) - 1) \
		    / sizeof (l4_word_t)];				\
    } __rla_arg2 = { (__rla_arg) };					\
    for (int __rla_i = 0;						\
	 __rla_i < sizeof (__rla_arg2) / sizeof (l4_word_t);		\
	 __rla_i ++)							\
      l4_msg_append_word (*msg, __rla_arg2.__rla_raw[__rla_i]);		\
  }

#define RPCLOAD0(...)
#define RPCLOAD1(__rl_type, __rl_arg, ...) RPCLOADARG(__rl_type, __rl_arg) RPCLOAD0(__VA_ARGS__)
#define RPCLOAD2(__rl_type, __rl_arg, ...) RPCLOADARG(__rl_type, __rl_arg) RPCLOAD1(__VA_ARGS__)
#define RPCLOAD3(__rl_type, __rl_arg, ...) RPCLOADARG(__rl_type, __rl_arg) RPCLOAD2(__VA_ARGS__)
#define RPCLOAD4(__rl_type, __rl_arg, ...) RPCLOADARG(__rl_type, __rl_arg) RPCLOAD3(__VA_ARGS__)
#define RPCLOAD5(__rl_type, __rl_arg, ...) RPCLOADARG(__rl_type, __rl_arg) RPCLOAD4(__VA_ARGS__)
#define RPCLOAD6(__rl_type, __rl_arg, ...) RPCLOADARG(__rl_type, __rl_arg) RPCLOAD5(__VA_ARGS__)
#define RPCLOAD7(__rl_type, __rl_arg, ...) RPCLOADARG(__rl_type, __rl_arg) RPCLOAD6(__VA_ARGS__)
#define RPCLOAD8(__rl_type, __rl_arg, ...) RPCLOADARG(__rl_type, __rl_arg) RPCLOAD7(__VA_ARGS__)
#define RPCLOAD9(__rl_type, __rl_arg, ...) RPCLOADARG(__rl_type, __rl_arg) RPCLOAD8(__VA_ARGS__)
#define RPCLOAD10(__rl_type, __rl_arg, ...) RPCLOADARG(__rl_type, __rl_arg) RPCLOAD9(__VA_ARGS__)
#define RPCLOAD11(__rl_type, __rl_arg, ...) RPCLOADARG(__rl_type, __rl_arg) RPCLOAD10(__VA_ARGS__)
#define RPCLOAD12(__rl_type, __rl_arg, ...) RPCLOADARG(__rl_type, __rl_arg) RPCLOAD11(__VA_ARGS__)
#define RPCLOAD13(__rl_type, __rl_arg, ...) RPCLOADARG(__rl_type, __rl_arg) RPCLOAD12(__VA_ARGS__)
#define RPCLOAD14(__rl_type, __rl_arg, ...) RPCLOADARG(__rl_type, __rl_arg) RPCLOAD13(__VA_ARGS__)
#define RPCLOAD15(__rl_type, __rl_arg, ...) RPCLOADARG(__rl_type, __rl_arg) RPCLOAD14(__VA_ARGS__)
#define RPCLOAD16(__rl_type, __rl_arg, ...) RPCLOADARG(__rl_type, __rl_arg) RPCLOAD15(__VA_ARGS__)
#define RPCLOAD17(__rl_type, __rl_arg, ...) RPCLOADARG(__rl_type, __rl_arg) RPCLOAD16(__VA_ARGS__)
#define RPCLOAD18(__rl_type, __rl_arg, ...) RPCLOADARG(__rl_type, __rl_arg) RPCLOAD17(__VA_ARGS__)
#define RPCLOAD19(__rl_type, __rl_arg, ...) RPCLOADARG(__rl_type, __rl_arg) RPCLOAD18(__VA_ARGS__)
#define RPCLOAD20(__rl_type, __rl_arg, ...) RPCLOADARG(__rl_type, __rl_arg) RPCLOAD19(__VA_ARGS__)
#define RPCLOAD21(__rl_type, __rl_arg, ...) RPCLOADARG(__rl_type, __rl_arg) RPCLOAD20(__VA_ARGS__)
#define RPCLOAD22(__rl_type, __rl_arg, ...) RPCLOADARG(__rl_type, __rl_arg) RPCLOAD21(__VA_ARGS__)
#define RPCLOAD23(__rl_type, __rl_arg, ...) RPCLOADARG(__rl_type, __rl_arg) RPCLOAD22(__VA_ARGS__)
#define RPCLOAD24(__rl_type, __rl_arg, ...) RPCLOADARG(__rl_type, __rl_arg) RPCLOAD23(__VA_ARGS__)
#define RPCLOAD25(__rl_type, __rl_arg, ...) RPCLOADARG(__rl_type, __rl_arg) RPCLOAD24(__VA_ARGS__)
#define RPCLOAD_(__rl_count, ...) RPCLOAD##__rl_count (__VA_ARGS__)
#define RPCLOAD(__rl_count, ...) RPCLOAD_ (__rl_count, __VA_ARGS__)

/* Store the contents of MR __RSU_IDX+1 into *ARG, which is of type TYPE.
   NB: __RSU_IDX is thus the return parameter number, not the message
   register number; MR0 contains the error code.  */
#define RPCSTOREARG(__rsa_type, __rsa_arg)				\
  {									\
    union								\
    {									\
      __rsa_type __rsa_a;						\
      l4_word_t __rsa_raw[(sizeof (__rsa_type) + sizeof (l4_word_t) - 1) \
			  / sizeof (l4_word_t)];			\
    } __rsa_arg2;							\
    for (int __rsa_i = 0;						\
	 __rsa_i < sizeof (__rsa_arg2) / sizeof (l4_word_t);		\
	 __rsa_i ++)							\
      __rsa_arg2.__rsa_raw[__rsa_i] = l4_msg_word (*msg, __rsu_idx ++);	\
    *(__rsa_arg) = __rsa_arg2.__rsa_a;					\
  }

#define RPCSTORE0(...)
#define RPCSTORE1(__rs_type, __rs_arg, ...) \
  RPCSTOREARG(__rs_type, __rs_arg) RPCSTORE0(__VA_ARGS__)
#define RPCSTORE2(__rs_type, __rs_arg, ...) \
  RPCSTOREARG(__rs_type, __rs_arg) RPCSTORE1(__VA_ARGS__)
#define RPCSTORE3(__rs_type, __rs_arg, ...) \
  RPCSTOREARG(__rs_type, __rs_arg) RPCSTORE2(__VA_ARGS__)
#define RPCSTORE4(__rs_type, __rs_arg, ...) \
  RPCSTOREARG(__rs_type, __rs_arg) RPCSTORE3(__VA_ARGS__)
#define RPCSTORE5(__rs_type, __rs_arg, ...) \
  RPCSTOREARG(__rs_type, __rs_arg) RPCSTORE4(__VA_ARGS__)
#define RPCSTORE6(__rs_type, __rs_arg, ...) \
  RPCSTOREARG(__rs_type, __rs_arg) RPCSTORE5(__VA_ARGS__)
#define RPCSTORE7(__rs_type, __rs_arg, ...) \
  RPCSTOREARG(__rs_type, __rs_arg) RPCSTORE6(__VA_ARGS__)
#define RPCSTORE8(__rs_type, __rs_arg, ...) \
  RPCSTOREARG(__rs_type, __rs_arg) RPCSTORE7(__VA_ARGS__)
#define RPCSTORE9(__rs_type, __rs_arg, ...) \
  RPCSTOREARG(__rs_type, __rs_arg) RPCSTORE8(__VA_ARGS__)
#define RPCSTORE10(__rs_type, __rs_arg, ...) \
  RPCSTOREARG(__rs_type, __rs_arg) RPCSTORE9(__VA_ARGS__)
#define RPCSTORE11(__rs_type, __rs_arg, ...) \
  RPCSTOREARG(__rs_type, __rs_arg) RPCSTORE10(__VA_ARGS__)
#define RPCSTORE12(__rs_type, __rs_arg, ...) \
  RPCSTOREARG(__rs_type, __rs_arg) RPCSTORE11(__VA_ARGS__)
#define RPCSTORE13(__rs_type, __rs_arg, ...) \
  RPCSTOREARG(__rs_type, __rs_arg) RPCSTORE12(__VA_ARGS__)
#define RPCSTORE14(__rs_type, __rs_arg, ...) \
  RPCSTOREARG(__rs_type, __rs_arg) RPCSTORE13(__VA_ARGS__)
#define RPCSTORE15(__rs_type, __rs_arg, ...) \
  RPCSTOREARG(__rs_type, __rs_arg) RPCSTORE14(__VA_ARGS__)
#define RPCSTORE16(__rs_type, __rs_arg, ...) \
  RPCSTOREARG(__rs_type, __rs_arg) RPCSTORE15(__VA_ARGS__)
#define RPCSTORE17(__rs_type, __rs_arg, ...) \
  RPCSTOREARG(__rs_type, __rs_arg) RPCSTORE16(__VA_ARGS__)
#define RPCSTORE18(__rs_type, __rs_arg, ...) \
  RPCSTOREARG(__rs_type, __rs_arg) RPCSTORE17(__VA_ARGS__)
#define RPCSTORE19(__rs_type, __rs_arg, ...) \
  RPCSTOREARG(__rs_type, __rs_arg) RPCSTORE18(__VA_ARGS__)
#define RPCSTORE20(__rs_type, __rs_arg, ...) \
  RPCSTOREARG(__rs_type, __rs_arg) RPCSTORE19(__VA_ARGS__)
#define RPCSTORE21(__rs_type, __rs_arg, ...) \
  RPCSTOREARG(__rs_type, __rs_arg) RPCSTORE20(__VA_ARGS__)
#define RPCSTORE22(__rs_type, __rs_arg, ...) \
  RPCSTOREARG(__rs_type, __rs_arg) RPCSTORE21(__VA_ARGS__)
#define RPCSTORE23(__rs_type, __rs_arg, ...) \
  RPCSTOREARG(__rs_type, __rs_arg) RPCSTORE22(__VA_ARGS__)
#define RPCSTORE24(__rs_type, __rs_arg, ...) \
  RPCSTOREARG(__rs_type, __rs_arg) RPCSTORE23(__VA_ARGS__)
#define RPCSTORE25(__rs_type, __rs_arg, ...) \
  RPCSTOREARG(__rs_type, __rs_arg) RPCSTORE24(__VA_ARGS__)

#define RPCSTORE_(__rs_count, ...) RPCSTORE##__rs_count (__VA_ARGS__)
#define RPCSTORE(__rs_count, ...) RPCSTORE_ (__rs_count, __VA_ARGS__)

/* Marshal the in-arguments into the provided message buffer.  */
#define RPC_SEND_MARSHAL(id, icount, ...)				\
  static inline void							\
  RPC_CONCAT (RPC_STUB_PREFIX_(id), _send_marshal)			\
    (l4_msg_t *msg RPC_IF_COMMA (icount) ()				\
     RPC_GRAB2 (, icount, ##__VA_ARGS__))				\
  {									\
    l4_msg_tag_t tag;							\
    									\
    tag = l4_niltag;							\
    l4_msg_tag_set_label (&tag, RPC_ID_PREFIX_(id));			\
    									\
    l4_msg_clear (*msg);						\
    l4_msg_set_msg_tag (*msg, tag);					\
    									\
    RPCLOAD (icount, ##__VA_ARGS__);					\
  }

/* Unmarshal the in-arguments from the provided message buffer.  */
#define RPC_SEND_UNMARSHAL(id, icount, ...)				\
  static inline error_t							\
  RPC_CONCAT (RPC_STUB_PREFIX_(id), _send_unmarshal)			\
    (l4_msg_t *msg RPC_IF_COMMA(icount) ()				\
     RPC_GRAB2 (*, icount, ##__VA_ARGS__))				\
  {									\
    l4_msg_tag_t tag = l4_msg_msg_tag (*msg);				\
									\
    l4_word_t label;							\
    label = l4_label (tag);						\
    if (label != RPC_ID_PREFIX_(id))					\
      {									\
	debug (1, #id " has bad method id, %d, excepted %d",		\
	       label, RPC_ID_PREFIX_(id));				\
	return EINVAL;							\
      }									\
    									\
    error_t err = 0;							\
    int __rsu_idx __attribute__ ((unused));				\
    __rsu_idx = 0;							\
    RPCSTORE (icount, ##__VA_ARGS__);					\
    if (err == 0 && __rsu_idx != l4_untyped_words (tag))		\
      {									\
	debug (1, #id " has wrong number of arguments: %d, expected %d words", \
	       l4_untyped_words (tag), __rsu_idx);			\
	return EINVAL;							\
      }									\
    return 0;								\
  }

/* Marshal the reply.  */
#define RPC_REPLY_MARSHAL(id, ocount, ...)				\
  static inline void							\
  RPC_CONCAT (RPC_STUB_PREFIX_(id), _reply_marshal)			\
    (l4_msg_t *msg RPC_IF_COMMA (ocount) ()				\
     RPC_GRAB2 (, ocount, ##__VA_ARGS__))				\
  {									\
    l4_msg_tag_t tag;							\
    									\
    tag = l4_niltag;							\
    l4_msg_tag_set_label (&tag, RPC_ID_PREFIX_(id));			\
    									\
    l4_msg_clear (*msg);						\
    l4_msg_set_msg_tag (*msg, tag);					\
									\
    /* No error.  */							\
    l4_msg_append_word (*msg, 0);					\
    									\
    RPCLOAD (ocount, ##__VA_ARGS__);					\
  }

/* Unmarshal the reply.  */
#define RPC_REPLY_UNMARSHAL(id, ocount, ...)				\
  static inline error_t							\
  RPC_CONCAT (RPC_STUB_PREFIX_(id), _reply_unmarshal)			\
    (l4_msg_t *msg RPC_IF_COMMA (ocount) ()				\
     RPC_GRAB2(*, ocount, ##__VA_ARGS__))				\
  {									\
    l4_msg_tag_t tag = l4_msg_msg_tag (*msg);				\
    l4_word_t err = l4_msg_word (*msg, 0);				\
    									\
    int __rsu_idx __attribute__ ((unused));				\
    __rsu_idx = 1;							\
    RPCSTORE (ocount, ##__VA_ARGS__);					\
    if (err == 0)							\
      {									\
	if (__rsu_idx != l4_untyped_words (tag))			\
	  {								\
	    debug (1, "Got %d words, expected %d words",		\
		   __rsu_idx, l4_untyped_words (tag));			\
	    return EINVAL;						\
	  }								\
      }									\
    return err;								\
  }

/* RPC_ARGUMENTS takes a list of types and arguments and returns the first
   COUNT arguments.  (NB: the list may contain more than COUNT
   arguments!).  */
#define RPC_ARGUMENTS0(...)
#define RPC_ARGUMENTS1(__ra_type, __ra_arg, ...) __ra_arg RPC_ARGUMENTS0(__VA_ARGS__)
#define RPC_ARGUMENTS2(__ra_type, __ra_arg, ...) __ra_arg, RPC_ARGUMENTS1(__VA_ARGS__)
#define RPC_ARGUMENTS3(__ra_type, __ra_arg, ...) __ra_arg, RPC_ARGUMENTS2(__VA_ARGS__)
#define RPC_ARGUMENTS4(__ra_type, __ra_arg, ...) __ra_arg, RPC_ARGUMENTS3(__VA_ARGS__)
#define RPC_ARGUMENTS5(__ra_type, __ra_arg, ...) __ra_arg, RPC_ARGUMENTS4(__VA_ARGS__)
#define RPC_ARGUMENTS6(__ra_type, __ra_arg, ...) __ra_arg, RPC_ARGUMENTS5(__VA_ARGS__)
#define RPC_ARGUMENTS7(__ra_type, __ra_arg, ...) __ra_arg, RPC_ARGUMENTS6(__VA_ARGS__)
#define RPC_ARGUMENTS8(__ra_type, __ra_arg, ...) __ra_arg, RPC_ARGUMENTS7(__VA_ARGS__)
#define RPC_ARGUMENTS9(__ra_type, __ra_arg, ...) __ra_arg, RPC_ARGUMENTS8(__VA_ARGS__)
#define RPC_ARGUMENTS10(__ra_type, __ra_arg, ...) __ra_arg, RPC_ARGUMENTS9(__VA_ARGS__)
#define RPC_ARGUMENTS11(__ra_type, __ra_arg, ...) __ra_arg, RPC_ARGUMENTS10(__VA_ARGS__)
#define RPC_ARGUMENTS12(__ra_type, __ra_arg, ...) __ra_arg, RPC_ARGUMENTS11(__VA_ARGS__)
#define RPC_ARGUMENTS13(__ra_type, __ra_arg, ...) __ra_arg, RPC_ARGUMENTS12(__VA_ARGS__)
#define RPC_ARGUMENTS14(__ra_type, __ra_arg, ...) __ra_arg, RPC_ARGUMENTS13(__VA_ARGS__)
#define RPC_ARGUMENTS15(__ra_type, __ra_arg, ...) __ra_arg, RPC_ARGUMENTS14(__VA_ARGS__)
#define RPC_ARGUMENTS16(__ra_type, __ra_arg, ...) __ra_arg, RPC_ARGUMENTS15(__VA_ARGS__)
#define RPC_ARGUMENTS17(__ra_type, __ra_arg, ...) __ra_arg, RPC_ARGUMENTS16(__VA_ARGS__)
#define RPC_ARGUMENTS18(__ra_type, __ra_arg, ...) __ra_arg, RPC_ARGUMENTS17(__VA_ARGS__)
#define RPC_ARGUMENTS19(__ra_type, __ra_arg, ...) __ra_arg, RPC_ARGUMENTS18(__VA_ARGS__)
#define RPC_ARGUMENTS20(__ra_type, __ra_arg, ...) __ra_arg, RPC_ARGUMENTS19(__VA_ARGS__)
#define RPC_ARGUMENTS_(__ra_count, ...) RPC_ARGUMENTS##__ra_count(__VA_ARGS__)
#define RPC_ARGUMENTS(__ra_count, ...) RPC_ARGUMENTS_(__ra_count, __VA_ARGS__)

/* Given a list of arguments, returns the arguments minus the first
   COUNT **pairs** of arguments.  For example:

     RPC_CHOP(1, int, i, int, j, double, d)

   =>

     int, j, double, d

  */
#define RPC_CHOP0(...) __VA_ARGS__
#define RPC_CHOP1(__rc_a, __rc_b, ...) RPC_CHOP0(__VA_ARGS__)
#define RPC_CHOP2(__rc_a, __rc_b, ...) RPC_CHOP1(__VA_ARGS__)
#define RPC_CHOP3(__rc_a, __rc_b, ...) RPC_CHOP2(__VA_ARGS__)
#define RPC_CHOP4(__rc_a, __rc_b, ...) RPC_CHOP3(__VA_ARGS__)
#define RPC_CHOP5(__rc_a, __rc_b, ...) RPC_CHOP4(__VA_ARGS__)
#define RPC_CHOP6(__rc_a, __rc_b, ...) RPC_CHOP5(__VA_ARGS__)
#define RPC_CHOP7(__rc_a, __rc_b, ...) RPC_CHOP6(__VA_ARGS__)
#define RPC_CHOP8(__rc_a, __rc_b, ...) RPC_CHOP7(__VA_ARGS__)
#define RPC_CHOP9(__rc_a, __rc_b, ...) RPC_CHOP8(__VA_ARGS__)
#define RPC_CHOP10(__rc_a, __rc_b, ...) RPC_CHOP9(__VA_ARGS__)
#define RPC_CHOP11(__rc_a, __rc_b, ...) RPC_CHOP10(__VA_ARGS__)
#define RPC_CHOP12(__rc_a, __rc_b, ...) RPC_CHOP11(__VA_ARGS__)
#define RPC_CHOP13(__rc_a, __rc_b, ...) RPC_CHOP12(__VA_ARGS__)
#define RPC_CHOP14(__rc_a, __rc_b, ...) RPC_CHOP13(__VA_ARGS__)
#define RPC_CHOP15(__rc_a, __rc_b, ...) RPC_CHOP14(__VA_ARGS__)
#define RPC_CHOP16(__rc_a, __rc_b, ...) RPC_CHOP15(__VA_ARGS__)
#define RPC_CHOP17(__rc_a, __rc_b, ...) RPC_CHOP16(__VA_ARGS__)
#define RPC_CHOP18(__rc_a, __rc_b, ...) RPC_CHOP17(__VA_ARGS__)
#define RPC_CHOP19(__rc_a, __rc_b, ...) RPC_CHOP18(__VA_ARGS__)
#define RPC_CHOP20(__rc_a, __rc_b, ...) RPC_CHOP19(__VA_ARGS__)
#define RPC_CHOP21(__rc_a, __rc_b, ...) RPC_CHOP20(__VA_ARGS__)
#define RPC_CHOP22(__rc_a, __rc_b, ...) RPC_CHOP21(__VA_ARGS__)
#define RPC_CHOP23(__rc_a, __rc_b, ...) RPC_CHOP22(__VA_ARGS__)
#define RPC_CHOP24(__rc_a, __rc_b, ...) RPC_CHOP23(__VA_ARGS__)
#define RPC_CHOP25(__rc_a, __rc_b, ...) RPC_CHOP24(__VA_ARGS__)
#define RPC_CHOP_(__rc_count, ...) RPC_CHOP##__rc_count (__VA_ARGS__)
#define RPC_CHOP(__rc_count, ...) RPC_CHOP_(__rc_count, __VA_ARGS__)

/* Given a list of arguments, returns the first COUNT **pairs** of
   arguments, the elements of each pair separated by SEP and each pair
   separated by a comma.  For example:

  For example:

     RPC_GRAB2(, 2, int, i, int, j, double, d)

   =>

     int i, int j
*/
#define RPC_GRAB2_0(__rg_sep, ...) 
#define RPC_GRAB2_1(__rg_sep, __rg_a, __rg_b, ...) __rg_a __rg_sep __rg_b RPC_GRAB2_0(__rg_sep, __VA_ARGS__)
#define RPC_GRAB2_2(__rg_sep, __rg_a, __rg_b, ...) __rg_a __rg_sep __rg_b, RPC_GRAB2_1(__rg_sep, __VA_ARGS__)
#define RPC_GRAB2_3(__rg_sep, __rg_a, __rg_b, ...) __rg_a __rg_sep __rg_b, RPC_GRAB2_2(__rg_sep, __VA_ARGS__)
#define RPC_GRAB2_4(__rg_sep, __rg_a, __rg_b, ...) __rg_a __rg_sep __rg_b, RPC_GRAB2_3(__rg_sep, __VA_ARGS__)
#define RPC_GRAB2_5(__rg_sep, __rg_a, __rg_b, ...) __rg_a __rg_sep __rg_b, RPC_GRAB2_4(__rg_sep, __VA_ARGS__)
#define RPC_GRAB2_6(__rg_sep, __rg_a, __rg_b, ...) __rg_a __rg_sep __rg_b, RPC_GRAB2_5(__rg_sep, __VA_ARGS__)
#define RPC_GRAB2_7(__rg_sep, __rg_a, __rg_b, ...) __rg_a __rg_sep __rg_b, RPC_GRAB2_6(__rg_sep, __VA_ARGS__)
#define RPC_GRAB2_8(__rg_sep, __rg_a, __rg_b, ...) __rg_a __rg_sep __rg_b, RPC_GRAB2_7(__rg_sep, __VA_ARGS__)
#define RPC_GRAB2_9(__rg_sep, __rg_a, __rg_b, ...) __rg_a __rg_sep __rg_b, RPC_GRAB2_8(__rg_sep, __VA_ARGS__)
#define RPC_GRAB2_10(__rg_sep, __rg_a, __rg_b, ...) __rg_a __rg_sep __rg_b, RPC_GRAB2_9(__rg_sep, __VA_ARGS__)
#define RPC_GRAB2_11(__rg_sep, __rg_a, __rg_b, ...) __rg_a __rg_sep __rg_b, RPC_GRAB2_10(__rg_sep, __VA_ARGS__)
#define RPC_GRAB2_12(__rg_sep, __rg_a, __rg_b, ...) __rg_a __rg_sep __rg_b, RPC_GRAB2_11(__rg_sep, __VA_ARGS__)
#define RPC_GRAB2_13(__rg_sep, __rg_a, __rg_b, ...) __rg_a __rg_sep __rg_b, RPC_GRAB2_12(__rg_sep, __VA_ARGS__)
#define RPC_GRAB2_14(__rg_sep, __rg_a, __rg_b, ...) __rg_a __rg_sep __rg_b, RPC_GRAB2_13(__rg_sep, __VA_ARGS__)
#define RPC_GRAB2_15(__rg_sep, __rg_a, __rg_b, ...) __rg_a __rg_sep __rg_b, RPC_GRAB2_14(__rg_sep, __VA_ARGS__)
#define RPC_GRAB2_16(__rg_sep, __rg_a, __rg_b, ...) __rg_a __rg_sep __rg_b, RPC_GRAB2_15(__rg_sep, __VA_ARGS__)
#define RPC_GRAB2_17(__rg_sep, __rg_a, __rg_b, ...) __rg_a __rg_sep __rg_b, RPC_GRAB2_16(__rg_sep, __VA_ARGS__)
#define RPC_GRAB2_18(__rg_sep, __rg_a, __rg_b, ...) __rg_a __rg_sep __rg_b, RPC_GRAB2_17(__rg_sep, __VA_ARGS__)
#define RPC_GRAB2_19(__rg_sep, __rg_a, __rg_b, ...) __rg_a __rg_sep __rg_b, RPC_GRAB2_18(__rg_sep, __VA_ARGS__)
#define RPC_GRAB2_20(__rg_sep, __rg_a, __rg_b, ...) __rg_a __rg_sep __rg_b, RPC_GRAB2_19(__rg_sep, __VA_ARGS__)
#define RPC_GRAB2_21(__rg_sep, __rg_a, __rg_b, ...) __rg_a __rg_sep __rg_b, RPC_GRAB2_20(__rg_sep, __VA_ARGS__)
#define RPC_GRAB2_22(__rg_sep, __rg_a, __rg_b, ...) __rg_a __rg_sep __rg_b, RPC_GRAB2_21(__rg_sep, __VA_ARGS__)
#define RPC_GRAB2_23(__rg_sep, __rg_a, __rg_b, ...) __rg_a __rg_sep __rg_b, RPC_GRAB2_22(__rg_sep, __VA_ARGS__)
#define RPC_GRAB2_24(__rg_sep, __rg_a, __rg_b, ...) __rg_a __rg_sep __rg_b, RPC_GRAB2_23(__rg_sep, __VA_ARGS__)
#define RPC_GRAB2_25(__rg_sep, __rg_a, __rg_b, ...) __rg_a __rg_sep __rg_b, RPC_GRAB2_24(__rg_sep, __VA_ARGS__)
#define RPC_GRAB2_(__rg_sep, __rg_count, ...) RPC_GRAB2_##__rg_count (__rg_sep, __VA_ARGS__)
#define RPC_GRAB2(__rg_sep, __rg_count, ...) RPC_GRAB2_(__rg_sep, __rg_count, __VA_ARGS__)

#define RPC_GRAB_0(...) 
#define RPC_GRAB_1(__rg_a, ...) __rg_a RPC_GRAB_0(__VA_ARGS__)
#define RPC_GRAB_2(__rg_a, ...) __rg_a, RPC_GRAB_1(__VA_ARGS__)
#define RPC_GRAB_3(__rg_a, ...) __rg_a, RPC_GRAB_2(__VA_ARGS__)
#define RPC_GRAB_4(__rg_a, ...) __rg_a, RPC_GRAB_3(__VA_ARGS__)
#define RPC_GRAB_5(__rg_a, ...) __rg_a, RPC_GRAB_4(__VA_ARGS__)
#define RPC_GRAB_6(__rg_a, ...) __rg_a, RPC_GRAB_5(__VA_ARGS__)
#define RPC_GRAB_7(__rg_a, ...) __rg_a, RPC_GRAB_6(__VA_ARGS__)
#define RPC_GRAB_8(__rg_a, ...) __rg_a, RPC_GRAB_7(__VA_ARGS__)
#define RPC_GRAB_9(__rg_a, ...) __rg_a, RPC_GRAB_8(__VA_ARGS__)
#define RPC_GRAB_10(__rg_a, ...) __rg_a, RPC_GRAB_9(__VA_ARGS__)
#define RPC_GRAB_11(__rg_a, ...) __rg_a, RPC_GRAB_10(__VA_ARGS__)
#define RPC_GRAB_12(__rg_a, ...) __rg_a, RPC_GRAB_11(__VA_ARGS__)
#define RPC_GRAB_13(__rg_a, ...) __rg_a, RPC_GRAB_12(__VA_ARGS__)
#define RPC_GRAB_14(__rg_a, ...) __rg_a, RPC_GRAB_13(__VA_ARGS__)
#define RPC_GRAB_15(__rg_a, ...) __rg_a, RPC_GRAB_14(__VA_ARGS__)
#define RPC_GRAB_16(__rg_a, ...) __rg_a, RPC_GRAB_15(__VA_ARGS__)
#define RPC_GRAB_17(__rg_a, ...) __rg_a, RPC_GRAB_16(__VA_ARGS__)
#define RPC_GRAB_18(__rg_a, ...) __rg_a, RPC_GRAB_17(__VA_ARGS__)
#define RPC_GRAB_19(__rg_a, ...) __rg_a, RPC_GRAB_18(__VA_ARGS__)
#define RPC_GRAB_20(__rg_a, ...) __rg_a, RPC_GRAB_19(__VA_ARGS__)
#define RPC_GRAB_21(__rg_a, ...) __rg_a, RPC_GRAB_20(__VA_ARGS__)
#define RPC_GRAB_22(__rg_a, ...) __rg_a, RPC_GRAB_21(__VA_ARGS__)
#define RPC_GRAB_23(__rg_a, ...) __rg_a, RPC_GRAB_22(__VA_ARGS__)
#define RPC_GRAB_24(__rg_a, ...) __rg_a, RPC_GRAB_23(__VA_ARGS__)
#define RPC_GRAB_25(__rg_a, ...) __rg_a, RPC_GRAB_24(__VA_ARGS__)
#define RPC_GRAB_(__rg_count, ...) RPC_GRAB_##__rg_count (__VA_ARGS__)
#define RPC_GRAB(__rg_count, ...) RPC_GRAB_(__rg_count, __VA_ARGS__)

/* Ensure that there are X pairs of arguments.  */
#define RPC_INVALID_NUMBER_OF_ARGUMENTS_
#define RPC_EMPTY_LIST_(x) RPC_INVALID_NUMBER_OF_ARGUMENTS_##x
#define RPC_EMPTY_LIST(x) RPC_EMPTY_LIST_(x)
#define RPC_ENSURE_ARGS(count, ...) \
  RPC_EMPTY_LIST (RPC_CHOP (count, __VA_ARGS__))

#define RPC_MARSHAL_GEN_(id, icount, ocount, ...)			\
  RPC_ENSURE_ARGS(ADD (icount, ocount), ##__VA_ARGS__)			\
									\
  RPC_SEND_MARSHAL(id, icount, ##__VA_ARGS__)				\
  RPC_SEND_UNMARSHAL(id, icount, ##__VA_ARGS__)				\
  RPC_REPLY_MARSHAL(id, ocount, RPC_CHOP (icount, ##__VA_ARGS__))	\
  RPC_REPLY_UNMARSHAL(id, ocount, RPC_CHOP (icount, ##__VA_ARGS__))

#define RPC_SIMPLE_(postfix, id, icount, ocount, ...)			\
  /* Send, but do not wait for a reply.  */				\
  static inline error_t							\
  __attribute__((always_inline))					\
  RPC_CONCAT(RPC_STUB_PREFIX_(id), postfix)				\
    (RPC_TARGET_ARG_ RPC_GRAB2 (, icount, __VA_ARGS__))			\
  {									\
    l4_msg_tag_t tag;							\
    l4_msg_t msg;							\
									\
    RPC_CONCAT (RPC_STUB_PREFIX_(id), _send_marshal)			\
      CPP_IFTHEN (icount,						\
		  (&msg, RPC_ARGUMENTS(icount, __VA_ARGS__)),		\
		  (&msg));						\
									\
    l4_msg_load (msg);							\
    l4_accept (L4_UNTYPED_WORDS_ACCEPTOR);				\
									\
    tag = l4_send (RPC_TARGET_);					\
    if (l4_ipc_failed (tag))						\
      return EHOSTDOWN;							\
									\
    return 0;								\
  }

#define RPC_(postfix, id, icount, ocount, ...)				\
  static inline error_t							\
  __attribute__((always_inline))					\
  RPC_CONCAT (RPC_STUB_PREFIX_(id), postfix)				\
    (RPC_TARGET_ARG_							\
     RPC_GRAB (ADD(icount, ocount),					\
	       RPC_GRAB2 (, icount, __VA_ARGS__) RPC_IF_COMMA(icount) () \
	       RPC_GRAB2 (*, ocount, RPC_CHOP (icount, __VA_ARGS__))))	\
  {									\
    l4_msg_tag_t tag;							\
    l4_msg_t msg;							\
									\
    RPC_CONCAT (RPC_STUB_PREFIX_(id), _send_marshal)			\
      CPP_IFTHEN (icount,						\
		  (&msg, RPC_ARGUMENTS(icount, __VA_ARGS__)),		\
		  (&msg));						\
									\
    l4_msg_load (msg);							\
    l4_accept (L4_UNTYPED_WORDS_ACCEPTOR);				\
									\
    tag = l4_call (RPC_TARGET_);					\
    if (l4_ipc_failed (tag))						\
      return EHOSTDOWN;							\
									\
    l4_msg_store (tag, msg);						\
    return RPC_CONCAT (RPC_STUB_PREFIX_(id), _reply_unmarshal)		\
      CPP_IFTHEN (ocount,						\
		  (&msg, RPC_ARGUMENTS (ocount,				\
					RPC_CHOP (icount, ##__VA_ARGS__))), \
		  (&msg));						\
  }									\

/* Generate stubs for marshalling a reply and sending it (without
   blocking).  */
#define RPC_REPLY_(id, icount, ocount, ...)				\
  static inline error_t							\
  RPC_CONCAT (RPC_STUB_PREFIX_(id), _reply)				\
    (l4_thread_id_t tid RPC_IF_COMMA (ocount) ()			\
     RPC_GRAB2 (, ocount, RPC_CHOP (icount, ##__VA_ARGS__)))		\
  {									\
    l4_msg_tag_t tag;							\
    l4_msg_t msg;							\
									\
    RPC_CONCAT (RPC_STUB_PREFIX_(id), _reply_marshal)			\
      (&msg RPC_IF_COMMA (ocount) ()					\
       RPC_ARGUMENTS(ocount, RPC_CHOP (icount, __VA_ARGS__)));		\
									\
    l4_msg_load (msg);							\
    l4_accept (L4_UNTYPED_WORDS_ACCEPTOR);				\
									\
    tag = l4_reply (tid);						\
    if (l4_ipc_failed (tag))						\
      return EHOSTDOWN;							\
    return 0;								\
  }

/* RPC template.  ID is the method name, ARGS is the list of arguments
   as normally passed to a function, LOADER is code to load the in
   parameters, and STORER is code to load the out parameters.  The
   code assumes that the first MR contains the error code and returns
   this as the function return value.  If the IPC fails, EHOSTDOWN is
   returned.  */

#define RPC_SIMPLE(id, icount, ocount, ...)		\
  RPC_MARSHAL_GEN_(id, icount, ocount, ##__VA_ARGS__)	\
  							\
  RPC_SIMPLE_(, id, icount, ocount, ##__VA_ARGS__)	\
  RPC_SIMPLE_(_send, id, icount, ocount, ##__VA_ARGS__)	\
  RPC_(_call, id, icount, ocount, ##__VA_ARGS__)	\
							\
  RPC_REPLY_(id, icount, ocount, ##__VA_ARGS__)

#define RPC(id, icount, ocount, ...)			\
  RPC_MARSHAL_GEN_(id, icount, ocount, ##__VA_ARGS__)	\
  							\
  RPC_(, id, icount, ocount, ##__VA_ARGS__)		\
  RPC_SIMPLE_(_send, id, icount, ocount, ##__VA_ARGS__)	\
  RPC_(_call, id, icount, ocount, ##__VA_ARGS__)	\
							\
  RPC_REPLY_(id, icount, ocount, ##__VA_ARGS__)

#endif
