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
#include <string.h>

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

/* Load the argument ARG, which is of type TYPE into MR IDX.  */
#define RPCLOADARG(type, arg)				     \
  {								     \
    assert ((sizeof (arg) & (sizeof (l4_word_t) - 1)) == 0);	     \
    l4_word_t raw[sizeof (arg) / sizeof (l4_word_t)];	     \
    memcpy (&raw[0], &arg, sizeof (raw));			     \
    for (int i_ = 0; i_ < sizeof (type) / sizeof (l4_word_t); i_ ++) \
      l4_msg_append_word (*msg, raw[i_]);			     \
  }

#define RPCLOAD0(...)
#define RPCLOAD1(type, arg, ...) RPCLOADARG(type, arg) RPCLOAD0(__VA_ARGS__)
#define RPCLOAD2(type, arg, ...) RPCLOADARG(type, arg) RPCLOAD1(__VA_ARGS__)
#define RPCLOAD3(type, arg, ...) RPCLOADARG(type, arg) RPCLOAD2(__VA_ARGS__)
#define RPCLOAD4(type, arg, ...) RPCLOADARG(type, arg) RPCLOAD3(__VA_ARGS__)
#define RPCLOAD5(type, arg, ...) RPCLOADARG(type, arg) RPCLOAD4(__VA_ARGS__)
#define RPCLOAD6(type, arg, ...) RPCLOADARG(type, arg) RPCLOAD5(__VA_ARGS__)
#define RPCLOAD7(type, arg, ...) RPCLOADARG(type, arg) RPCLOAD6(__VA_ARGS__)
#define RPCLOAD8(type, arg, ...) RPCLOADARG(type, arg) RPCLOAD7(__VA_ARGS__)
#define RPCLOAD9(type, arg, ...) RPCLOADARG(type, arg) RPCLOAD8(__VA_ARGS__)
#define RPCLOAD10(type, arg, ...) RPCLOADARG(type, arg) RPCLOAD9(__VA_ARGS__)
#define RPCLOAD11(type, arg, ...) RPCLOADARG(type, arg) RPCLOAD10(__VA_ARGS__)
#define RPCLOAD12(type, arg, ...) RPCLOADARG(type, arg) RPCLOAD11(__VA_ARGS__)
#define RPCLOAD13(type, arg, ...) RPCLOADARG(type, arg) RPCLOAD12(__VA_ARGS__)
#define RPCLOAD14(type, arg, ...) RPCLOADARG(type, arg) RPCLOAD13(__VA_ARGS__)
#define RPCLOAD15(type, arg, ...) RPCLOADARG(type, arg) RPCLOAD14(__VA_ARGS__)
#define RPCLOAD16(type, arg, ...) RPCLOADARG(type, arg) RPCLOAD15(__VA_ARGS__)
#define RPCLOAD17(type, arg, ...) RPCLOADARG(type, arg) RPCLOAD16(__VA_ARGS__)
#define RPCLOAD18(type, arg, ...) RPCLOADARG(type, arg) RPCLOAD17(__VA_ARGS__)
#define RPCLOAD19(type, arg, ...) RPCLOADARG(type, arg) RPCLOAD18(__VA_ARGS__)
#define RPCLOAD20(type, arg, ...) RPCLOADARG(type, arg) RPCLOAD19(__VA_ARGS__)
#define RPCLOAD21(type, arg, ...) RPCLOADARG(type, arg) RPCLOAD20(__VA_ARGS__)
#define RPCLOAD22(type, arg, ...) RPCLOADARG(type, arg) RPCLOAD21(__VA_ARGS__)
#define RPCLOAD23(type, arg, ...) RPCLOADARG(type, arg) RPCLOAD22(__VA_ARGS__)
#define RPCLOAD24(type, arg, ...) RPCLOADARG(type, arg) RPCLOAD23(__VA_ARGS__)
#define RPCLOAD25(type, arg, ...) RPCLOADARG(type, arg) RPCLOAD24(__VA_ARGS__)
#define RPCLOAD_(count, ...) RPCLOAD##count (__VA_ARGS__)
#define RPCLOAD(count, ...) RPCLOAD_ (count, __VA_ARGS__)

/* Store the contents of MR IDX+1 into *ARG, which is of type TYPE.
   NB: IDX is thus the return parameter number, not the message
   register number; MR0 contains the error code.  */
#define RPCSTOREARG(type, arg) \
  { \
    assert ((sizeof (*arg) & (sizeof (l4_word_t) - 1)) == 0); \
    union \
      { \
        type *a__; \
        l4_word_t *raw; \
      } arg_union_ = { (arg) }; \
    for (int i_ = 0; i_ < sizeof (*arg) / sizeof (l4_word_t); i_ ++) \
      arg_union_.raw[i_] = l4_msg_word (*msg, idx ++); \
  }

#define RPCSTORE0(...)
#define RPCSTORE1(type, arg, ...) \
  RPCSTOREARG(type, arg) RPCSTORE0(__VA_ARGS__)
#define RPCSTORE2(type, arg, ...) \
  RPCSTOREARG(type, arg) RPCSTORE1(__VA_ARGS__)
#define RPCSTORE3(type, arg, ...) \
  RPCSTOREARG(type, arg) RPCSTORE2(__VA_ARGS__)
#define RPCSTORE4(type, arg, ...) \
  RPCSTOREARG(type, arg) RPCSTORE3(__VA_ARGS__)
#define RPCSTORE5(type, arg, ...) \
  RPCSTOREARG(type, arg) RPCSTORE4(__VA_ARGS__)
#define RPCSTORE6(type, arg, ...) \
  RPCSTOREARG(type, arg) RPCSTORE5(__VA_ARGS__)
#define RPCSTORE7(type, arg, ...) \
  RPCSTOREARG(type, arg) RPCSTORE6(__VA_ARGS__)
#define RPCSTORE8(type, arg, ...) \
  RPCSTOREARG(type, arg) RPCSTORE7(__VA_ARGS__)
#define RPCSTORE9(type, arg, ...) \
  RPCSTOREARG(type, arg) RPCSTORE8(__VA_ARGS__)
#define RPCSTORE10(type, arg, ...) \
  RPCSTOREARG(type, arg) RPCSTORE9(__VA_ARGS__)
#define RPCSTORE11(type, arg, ...) \
  RPCSTOREARG(type, arg) RPCSTORE10(__VA_ARGS__)
#define RPCSTORE12(type, arg, ...) \
  RPCSTOREARG(type, arg) RPCSTORE11(__VA_ARGS__)
#define RPCSTORE13(type, arg, ...) \
  RPCSTOREARG(type, arg) RPCSTORE12(__VA_ARGS__)
#define RPCSTORE14(type, arg, ...) \
  RPCSTOREARG(type, arg) RPCSTORE13(__VA_ARGS__)
#define RPCSTORE15(type, arg, ...) \
  RPCSTOREARG(type, arg) RPCSTORE14(__VA_ARGS__)
#define RPCSTORE16(type, arg, ...) \
  RPCSTOREARG(type, arg) RPCSTORE15(__VA_ARGS__)
#define RPCSTORE17(type, arg, ...) \
  RPCSTOREARG(type, arg) RPCSTORE16(__VA_ARGS__)
#define RPCSTORE18(type, arg, ...) \
  RPCSTOREARG(type, arg) RPCSTORE17(__VA_ARGS__)
#define RPCSTORE19(type, arg, ...) \
  RPCSTOREARG(type, arg) RPCSTORE18(__VA_ARGS__)
#define RPCSTORE20(type, arg, ...) \
  RPCSTOREARG(type, arg) RPCSTORE19(__VA_ARGS__)
#define RPCSTORE21(type, arg, ...) \
  RPCSTOREARG(type, arg) RPCSTORE20(__VA_ARGS__)
#define RPCSTORE22(type, arg, ...) \
  RPCSTOREARG(type, arg) RPCSTORE21(__VA_ARGS__)
#define RPCSTORE23(type, arg, ...) \
  RPCSTOREARG(type, arg) RPCSTORE22(__VA_ARGS__)
#define RPCSTORE24(type, arg, ...) \
  RPCSTOREARG(type, arg) RPCSTORE23(__VA_ARGS__)
#define RPCSTORE25(type, arg, ...) \
  RPCSTOREARG(type, arg) RPCSTORE24(__VA_ARGS__)

#define RPCSTORE_(count, ...) RPCSTORE##count (__VA_ARGS__)
#define RPCSTORE(count, ...) RPCSTORE_ (count, __VA_ARGS__)

/* Marshal the in-arguments into the provided message buffer.  */
#define RPC_SEND_MARSHAL(id, icount, ...)				\
  static inline void							\
  RPC_CONCAT (RPC_STUB_PREFIX_(id), _send_marshal)			\
    CPP_IFTHEN (icount,							\
		(l4_msg_t *msg, RPC_GRAB2 (, icount, ##__VA_ARGS__)),	\
		(l4_msg_t *msg))					\
  {									\
    l4_msg_tag_t tag;							\
    									\
    tag = l4_niltag;							\
    l4_msg_tag_set_label (&tag, RPC_ID_PREFIX_(id));			\
    									\
    l4_msg_clear (*msg);						\
    l4_msg_set_msg_tag (*msg, tag);					\
    									\
    RPCLOAD (, icount, ##__VA_ARGS__);					\
  }

/* Unmarshal the in-arguments from the provided message buffer.  */
#define RPC_SEND_UNMARSHAL(id, icount, ...)				\
  static inline error_t							\
  RPC_CONCAT (RPC_STUB_PREFIX_(id), _send_unmarshal)			\
    CPP_IFTHEN (icount,							\
		(l4_msg_t *msg, RPC_GRAB2 (*, icount, ##__VA_ARGS__)),	\
		(l4_msg_t *msg))					\
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
    int idx __attribute__ ((unused));					\
    idx = 0;								\
    RPCSTORE (icount, ##__VA_ARGS__);					\
    if (err == 0 && idx != l4_untyped_words (tag))			\
      {									\
	debug (1, #id " has wrong number of arguments: %d, expected %d", \
	       l4_untyped_words (tag), idx);				\
	return EINVAL;							\
      }									\
    return 0;								\
  }

/* Marshal the reply.  */
#define RPC_REPLY_MARSHAL(id, ocount, ...)				\
  static inline void							\
  RPC_CONCAT (RPC_STUB_PREFIX_(id), _reply_marshal)			\
    CPP_IFTHEN (ocount,							\
		(l4_msg_t *msg, RPC_GRAB2 (, ocount, ##__VA_ARGS__)),	\
		(l4_msg_t *msg))					\
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
    RPCLOAD (, ocount, ##__VA_ARGS__);					\
  }

/* Unmarshal the reply.  */
#define RPC_REPLY_UNMARSHAL(id, ocount, ...)				\
  static inline error_t							\
  RPC_CONCAT (RPC_STUB_PREFIX_(id), _reply_unmarshal)			\
    CPP_IFTHEN (ocount,							\
		(l4_msg_t *msg, RPC_GRAB2(*, ocount, ##__VA_ARGS__)),	\
		(l4_msg_t *msg))					\
  {									\
    l4_msg_tag_t tag = l4_msg_msg_tag (*msg);				\
    l4_word_t err = l4_msg_word (*msg, 0);				\
    									\
    int idx __attribute__ ((unused));					\
    idx = 1;								\
    RPCSTORE (ocount, ##__VA_ARGS__);					\
    if (err == 0)							\
      {									\
	if (idx != l4_untyped_words (tag))				\
	  {								\
	    debug (1, "Got %d words, expected %d",			\
		   idx, l4_untyped_words (tag));			\
	    return EINVAL;						\
	  }								\
      }									\
    return err;								\
  }

/* RPC_ARGUMENTS takes a list of types and arguments and returns the first
   COUNT arguments.  (NB: the list may contain more than COUNT
   arguments!).  */
#define RPC_ARGUMENTS0(...)
#define RPC_ARGUMENTS1(type, arg, ...) arg RPC_ARGUMENTS0(__VA_ARGS__)
#define RPC_ARGUMENTS2(type, arg, ...) arg, RPC_ARGUMENTS1(__VA_ARGS__)
#define RPC_ARGUMENTS3(type, arg, ...) arg, RPC_ARGUMENTS2(__VA_ARGS__)
#define RPC_ARGUMENTS4(type, arg, ...) arg, RPC_ARGUMENTS3(__VA_ARGS__)
#define RPC_ARGUMENTS5(type, arg, ...) arg, RPC_ARGUMENTS4(__VA_ARGS__)
#define RPC_ARGUMENTS6(type, arg, ...) arg, RPC_ARGUMENTS5(__VA_ARGS__)
#define RPC_ARGUMENTS7(type, arg, ...) arg, RPC_ARGUMENTS6(__VA_ARGS__)
#define RPC_ARGUMENTS8(type, arg, ...) arg, RPC_ARGUMENTS7(__VA_ARGS__)
#define RPC_ARGUMENTS9(type, arg, ...) arg, RPC_ARGUMENTS8(__VA_ARGS__)
#define RPC_ARGUMENTS10(type, arg, ...) arg, RPC_ARGUMENTS9(__VA_ARGS__)
#define RPC_ARGUMENTS11(type, arg, ...) arg, RPC_ARGUMENTS10(__VA_ARGS__)
#define RPC_ARGUMENTS12(type, arg, ...) arg, RPC_ARGUMENTS11(__VA_ARGS__)
#define RPC_ARGUMENTS13(type, arg, ...) arg, RPC_ARGUMENTS12(__VA_ARGS__)
#define RPC_ARGUMENTS14(type, arg, ...) arg, RPC_ARGUMENTS13(__VA_ARGS__)
#define RPC_ARGUMENTS15(type, arg, ...) arg, RPC_ARGUMENTS14(__VA_ARGS__)
#define RPC_ARGUMENTS16(type, arg, ...) arg, RPC_ARGUMENTS15(__VA_ARGS__)
#define RPC_ARGUMENTS17(type, arg, ...) arg, RPC_ARGUMENTS16(__VA_ARGS__)
#define RPC_ARGUMENTS18(type, arg, ...) arg, RPC_ARGUMENTS17(__VA_ARGS__)
#define RPC_ARGUMENTS19(type, arg, ...) arg, RPC_ARGUMENTS18(__VA_ARGS__)
#define RPC_ARGUMENTS20(type, arg, ...) arg, RPC_ARGUMENTS19(__VA_ARGS__)
#define RPC_ARGUMENTS_(count, ...) RPC_ARGUMENTS##count(__VA_ARGS__)
#define RPC_ARGUMENTS(count, ...) RPC_ARGUMENTS_(count, __VA_ARGS__)

/* Given a list of arguments, returns the arguments minus the first
   COUNT **pairs** of arguments.  For example:

     RPC_CHOP(1, int, i, int, j, double, d)

   =>

     int, j, double, d

  */
#define RPC_CHOP0(...) __VA_ARGS__
#define RPC_CHOP1(a, b, ...) RPC_CHOP0(__VA_ARGS__)
#define RPC_CHOP2(a, b, ...) RPC_CHOP1(__VA_ARGS__)
#define RPC_CHOP3(a, b, ...) RPC_CHOP2(__VA_ARGS__)
#define RPC_CHOP4(a, b, ...) RPC_CHOP3(__VA_ARGS__)
#define RPC_CHOP5(a, b, ...) RPC_CHOP4(__VA_ARGS__)
#define RPC_CHOP6(a, b, ...) RPC_CHOP5(__VA_ARGS__)
#define RPC_CHOP7(a, b, ...) RPC_CHOP6(__VA_ARGS__)
#define RPC_CHOP8(a, b, ...) RPC_CHOP7(__VA_ARGS__)
#define RPC_CHOP9(a, b, ...) RPC_CHOP8(__VA_ARGS__)
#define RPC_CHOP10(a, b, ...) RPC_CHOP9(__VA_ARGS__)
#define RPC_CHOP11(a, b, ...) RPC_CHOP10(__VA_ARGS__)
#define RPC_CHOP12(a, b, ...) RPC_CHOP11(__VA_ARGS__)
#define RPC_CHOP13(a, b, ...) RPC_CHOP12(__VA_ARGS__)
#define RPC_CHOP14(a, b, ...) RPC_CHOP13(__VA_ARGS__)
#define RPC_CHOP15(a, b, ...) RPC_CHOP14(__VA_ARGS__)
#define RPC_CHOP16(a, b, ...) RPC_CHOP15(__VA_ARGS__)
#define RPC_CHOP17(a, b, ...) RPC_CHOP16(__VA_ARGS__)
#define RPC_CHOP18(a, b, ...) RPC_CHOP17(__VA_ARGS__)
#define RPC_CHOP19(a, b, ...) RPC_CHOP18(__VA_ARGS__)
#define RPC_CHOP20(a, b, ...) RPC_CHOP19(__VA_ARGS__)
#define RPC_CHOP21(a, b, ...) RPC_CHOP20(__VA_ARGS__)
#define RPC_CHOP22(a, b, ...) RPC_CHOP21(__VA_ARGS__)
#define RPC_CHOP23(a, b, ...) RPC_CHOP22(__VA_ARGS__)
#define RPC_CHOP24(a, b, ...) RPC_CHOP23(__VA_ARGS__)
#define RPC_CHOP25(a, b, ...) RPC_CHOP24(__VA_ARGS__)
#define RPC_CHOP_(count, ...) RPC_CHOP##count (__VA_ARGS__)
#define RPC_CHOP(count, ...) RPC_CHOP_(count, __VA_ARGS__)

/* Given a list of arguments, returns the first COUNT **pairs** of
   arguments, the elements of each pair separated by SEP and each pair
   separated by a comma.  For example:

  For example:

     RPC_GRAB2(, 2, int, i, int, j, double, d)

   =>

     int i, int j
*/
#define RPC_GRAB2_0(sep, ...) 
#define RPC_GRAB2_1(sep, a, b, ...) a sep b RPC_GRAB2_0(sep, __VA_ARGS__)
#define RPC_GRAB2_2(sep, a, b, ...) a sep b, RPC_GRAB2_1(sep, __VA_ARGS__)
#define RPC_GRAB2_3(sep, a, b, ...) a sep b, RPC_GRAB2_2(sep, __VA_ARGS__)
#define RPC_GRAB2_4(sep, a, b, ...) a sep b, RPC_GRAB2_3(sep, __VA_ARGS__)
#define RPC_GRAB2_5(sep, a, b, ...) a sep b, RPC_GRAB2_4(sep, __VA_ARGS__)
#define RPC_GRAB2_6(sep, a, b, ...) a sep b, RPC_GRAB2_5(sep, __VA_ARGS__)
#define RPC_GRAB2_7(sep, a, b, ...) a sep b, RPC_GRAB2_6(sep, __VA_ARGS__)
#define RPC_GRAB2_8(sep, a, b, ...) a sep b, RPC_GRAB2_7(sep, __VA_ARGS__)
#define RPC_GRAB2_9(sep, a, b, ...) a sep b, RPC_GRAB2_8(sep, __VA_ARGS__)
#define RPC_GRAB2_10(sep, a, b, ...) a sep b, RPC_GRAB2_9(sep, __VA_ARGS__)
#define RPC_GRAB2_11(sep, a, b, ...) a sep b, RPC_GRAB2_10(sep, __VA_ARGS__)
#define RPC_GRAB2_12(sep, a, b, ...) a sep b, RPC_GRAB2_11(sep, __VA_ARGS__)
#define RPC_GRAB2_13(sep, a, b, ...) a sep b, RPC_GRAB2_12(sep, __VA_ARGS__)
#define RPC_GRAB2_14(sep, a, b, ...) a sep b, RPC_GRAB2_13(sep, __VA_ARGS__)
#define RPC_GRAB2_15(sep, a, b, ...) a sep b, RPC_GRAB2_14(sep, __VA_ARGS__)
#define RPC_GRAB2_16(sep, a, b, ...) a sep b, RPC_GRAB2_15(sep, __VA_ARGS__)
#define RPC_GRAB2_17(sep, a, b, ...) a sep b, RPC_GRAB2_16(sep, __VA_ARGS__)
#define RPC_GRAB2_18(sep, a, b, ...) a sep b, RPC_GRAB2_17(sep, __VA_ARGS__)
#define RPC_GRAB2_19(sep, a, b, ...) a sep b, RPC_GRAB2_18(sep, __VA_ARGS__)
#define RPC_GRAB2_20(sep, a, b, ...) a sep b, RPC_GRAB2_19(sep, __VA_ARGS__)
#define RPC_GRAB2_21(sep, a, b, ...) a sep b, RPC_GRAB2_20(sep, __VA_ARGS__)
#define RPC_GRAB2_22(sep, a, b, ...) a sep b, RPC_GRAB2_21(sep, __VA_ARGS__)
#define RPC_GRAB2_23(sep, a, b, ...) a sep b, RPC_GRAB2_22(sep, __VA_ARGS__)
#define RPC_GRAB2_24(sep, a, b, ...) a sep b, RPC_GRAB2_23(sep, __VA_ARGS__)
#define RPC_GRAB2_25(sep, a, b, ...) a sep b, RPC_GRAB2_24(sep, __VA_ARGS__)
#define RPC_GRAB2_(sep, count, ...) RPC_GRAB2_##count (sep, __VA_ARGS__)
#define RPC_GRAB2(sep, count, ...) RPC_GRAB2_(sep, count, __VA_ARGS__)

#define RPC_GRAB_0(...) 
#define RPC_GRAB_1(a, ...) a RPC_GRAB_0(__VA_ARGS__)
#define RPC_GRAB_2(a, ...) a, RPC_GRAB_1(__VA_ARGS__)
#define RPC_GRAB_3(a, ...) a, RPC_GRAB_2(__VA_ARGS__)
#define RPC_GRAB_4(a, ...) a, RPC_GRAB_3(__VA_ARGS__)
#define RPC_GRAB_5(a, ...) a, RPC_GRAB_4(__VA_ARGS__)
#define RPC_GRAB_6(a, ...) a, RPC_GRAB_5(__VA_ARGS__)
#define RPC_GRAB_7(a, ...) a, RPC_GRAB_6(__VA_ARGS__)
#define RPC_GRAB_8(a, ...) a, RPC_GRAB_7(__VA_ARGS__)
#define RPC_GRAB_9(a, ...) a, RPC_GRAB_8(__VA_ARGS__)
#define RPC_GRAB_10(a, ...) a, RPC_GRAB_9(__VA_ARGS__)
#define RPC_GRAB_11(a, ...) a, RPC_GRAB_10(__VA_ARGS__)
#define RPC_GRAB_12(a, ...) a, RPC_GRAB_11(__VA_ARGS__)
#define RPC_GRAB_13(a, ...) a, RPC_GRAB_12(__VA_ARGS__)
#define RPC_GRAB_14(a, ...) a, RPC_GRAB_13(__VA_ARGS__)
#define RPC_GRAB_15(a, ...) a, RPC_GRAB_14(__VA_ARGS__)
#define RPC_GRAB_16(a, ...) a, RPC_GRAB_15(__VA_ARGS__)
#define RPC_GRAB_17(a, ...) a, RPC_GRAB_16(__VA_ARGS__)
#define RPC_GRAB_18(a, ...) a, RPC_GRAB_17(__VA_ARGS__)
#define RPC_GRAB_19(a, ...) a, RPC_GRAB_18(__VA_ARGS__)
#define RPC_GRAB_20(a, ...) a, RPC_GRAB_19(__VA_ARGS__)
#define RPC_GRAB_21(a, ...) a, RPC_GRAB_20(__VA_ARGS__)
#define RPC_GRAB_22(a, ...) a, RPC_GRAB_21(__VA_ARGS__)
#define RPC_GRAB_23(a, ...) a, RPC_GRAB_22(__VA_ARGS__)
#define RPC_GRAB_24(a, ...) a, RPC_GRAB_23(__VA_ARGS__)
#define RPC_GRAB_25(a, ...) a, RPC_GRAB_24(__VA_ARGS__)
#define RPC_GRAB_(count, ...) RPC_GRAB_##count (__VA_ARGS__)
#define RPC_GRAB(count, ...) RPC_GRAB_(count, __VA_ARGS__)

/* Ensure that there are X pairs of arguments.  */
#define RPC_INVALID_NUMBER_OF_ARGUMENTS_
#define RPC_EMPTY_LIST_(x) RPC_INVALID_NUMBER_OF_ARGUMENTS_##x
#define RPC_EMPTY_LIST(x) RPC_EMPTY_LIST_(x)
#define RPC_ENSURE_ARGS(count, ...) \
  RPC_EMPTY_LIST (RPC_CHOP (count, __VA_ARGS__))

/* RPC template.  ID is the method name, ARGS is the list of arguments
   as normally passed to a function, LOADER is code to load the in
   parameters, and STORER is code to load the out parameters.  The
   code assumes that the first MR contains the error code and returns
   this as the function return value.  If the IPC fails, EHOSTDOWN is
   returned.  */
#define RPC(id, icount, ocount, ...)					\
  RPC_ENSURE_ARGS(ADD (icount, ocount), ##__VA_ARGS__)			\
  RPC_SEND_MARSHAL(id, icount, ##__VA_ARGS__)				\
  RPC_SEND_UNMARSHAL(id, icount, ##__VA_ARGS__)				\
  RPC_REPLY_MARSHAL(id, ocount, RPC_CHOP (icount, ##__VA_ARGS__))	\
  RPC_REPLY_UNMARSHAL(id, ocount, RPC_CHOP (icount, ##__VA_ARGS__))	\
									\
  static inline error_t							\
  __attribute__((always_inline))					\
  RPC_STUB_PREFIX_(id) (RPC_TARGET_ARG_					\
			RPC_GRAB (ADD(icount, ocount),			\
				  RPC_GRAB2 (, icount, __VA_ARGS__),	\
				  RPC_GRAB2 (*, ocount,			\
					     RPC_CHOP (icount,		\
						       __VA_ARGS__))))	\
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
  }

#endif
