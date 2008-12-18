/* rpc.h - RPC template definitions.
   Copyright (C) 2007, 2008 Free Software Foundation, Inc.
   Written by Neal H. Walfield <neal@gnu.org>.

   This file is part of the GNU Hurd.

   GNU Hurd is free software: you can redistribute it and/or modify it
   under the terms of the GNU Lesser General Public License as
   published by the Free Software Foundation, either version 3 of the
   License, or (at your option) any later version.

   GNU Hurd is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with GNU Hurd.  If not, see
   <http://www.gnu.org/licenses/>.  */

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

#ifndef _HURD_RPC_H
#define _HURD_RPC_H

#include <hurd/stddef.h>
#include <viengoos/message.h>
#include <viengoos/ipc.h>
#include <errno.h>

#ifdef RM_INTERN
extern struct vg_message *reply_buffer;

/* We can't include messenger.h as it includes hurd/cap.h which in turn
   includes this file.  */
struct messenger;
struct activity;
extern bool messenger_message_load (struct activity *activity,
				    struct messenger *target,
				    struct vg_message *message);
#else
# include <hurd/message-buffer.h>
#endif
typedef vg_addr_t cap_t;

/* First we define some cpp help macros.  */
#define CPP_IFELSE_0(when, whennot) whennot
#define CPP_IFELSE_1(when, whennot) when
#define CPP_IFELSE_2(when, whennot) when
#define CPP_IFELSE_3(when, whennot) when
#define CPP_IFELSE_4(when, whennot) when
#define CPP_IFELSE_5(when, whennot) when
#define CPP_IFELSE_6(when, whennot) when
#define CPP_IFELSE_7(when, whennot) when
#define CPP_IFELSE_8(when, whennot) when
#define CPP_IFELSE_9(when, whennot) when
#define CPP_IFELSE_10(when, whennot) when
#define CPP_IFELSE_11(when, whennot) when
#define CPP_IFELSE_12(when, whennot) when
#define CPP_IFELSE_13(when, whennot) when
#define CPP_IFELSE_14(when, whennot) when
#define CPP_IFELSE_15(when, whennot) when
#define CPP_IFELSE_16(when, whennot) when
#define CPP_IFELSE_17(when, whennot) when
#define CPP_IFELSE_18(when, whennot) when
#define CPP_IFELSE_19(when, whennot) when
#define CPP_IFELSE_20(when, whennot) when
#define CPP_IFELSE_21(when, whennot) when
#define CPP_IFELSE_22(when, whennot) when
#define CPP_IFELSE_23(when, whennot) when
#define CPP_IFELSE_24(when, whennot) when
#define CPP_IFELSE_25(when, whennot) when

#define CPP_IFELSE_(expr, when, whennot)	\
  CPP_IFELSE_##expr(when, whennot)
#define CPP_IFELSE(expr, when, whennot)		\
  CPP_IFELSE_(expr, when, whennot)
#define CPP_IF(expr, when)			\
  CPP_IFELSE(expr, when,)
#define CPP_IFNOT(expr, whennot)		\
  CPP_IFELSE(expr, , whennot)

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

/* We'd like to define CPP_ADD as:

    #define CPP_ADD(x, y) \
      CPP_IFELSE(y, CPP_ADD(SUCC(x), SUCC(y)), y)

  This does not work as while a macro is being expanded, it becomes
  ineligible for expansion.  Thus, any references (including indirect
  references) are not expanded.  Repeated applications of a macro are,
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

#define CPP_ADD(x, y)				\
  CPP_IFELSE(y, CPP_APPLY##y(CPP_SUCC, x), x)

/* Apply a function to each of the first n arguments.


     CPP_FOREACH(2, CPP_SAFE_DEREF, NULL, a, b)

   =>

     ((a) ? *(a) : NULL), ((b) ? *(b) : NULL)
 */
#define CPP_FOREACH_0(func, cookie, ...)
#define CPP_FOREACH_1(func, cookie, element, ...) func(cookie, element)
#define CPP_FOREACH_2(func, cookie, element, ...) func(cookie, element), CPP_FOREACH_1(func, cookie, __VA_ARGS__)
#define CPP_FOREACH_3(func, cookie, element, ...) func(cookie, element), CPP_FOREACH_2(func, cookie, __VA_ARGS__)
#define CPP_FOREACH_4(func, cookie, element, ...) func(cookie, element), CPP_FOREACH_3(func, cookie, __VA_ARGS__)
#define CPP_FOREACH_5(func, cookie, element, ...) func(cookie, element), CPP_FOREACH_4(func, cookie, __VA_ARGS__)
#define CPP_FOREACH_6(func, cookie, element, ...) func(cookie, element), CPP_FOREACH_5(func, cookie, __VA_ARGS__)
#define CPP_FOREACH_7(func, cookie, element, ...) func(cookie, element), CPP_FOREACH_6(func, cookie, __VA_ARGS__)
#define CPP_FOREACH_8(func, cookie, element, ...) func(cookie, element), CPP_FOREACH_7(func, cookie, __VA_ARGS__)
#define CPP_FOREACH_9(func, cookie, element, ...) func(cookie, element), CPP_FOREACH_8(func, cookie, __VA_ARGS__)

#define CPP_FOREACH_(n, func, cookie, ...)	\
  CPP_FOREACH_##n(func, cookie, __VA_ARGS__)
#define CPP_FOREACH(n, func, cookie, ...)	\
  CPP_FOREACH_(n, func, cookie, __VA_ARGS__)

/* Used in conjunction with CPP_FOREACH.  Generates C code that
   dereferences ELEMENT if it is not NULL, otherwise, returns
   COOKIE.  */
#define CPP_SAFE_DEREF(cookie, element) ((element) ? *(element) : (cookie))


/* CPP treats commas specially so we have to be smart about how we
   insert them algorithmically.  For instance, this won't work:

   #define COMMA ,
     CPP_IFELSE(x, COMMA, )

   To optional insert a comma, use this function instead.  When the
   result is need, invoke the result.  For instance:

   RPC_IF_COMMA(x) ()
 */
#define RPC_COMMA() ,
#define RPC_NOCOMMA()
#define RPC_IF_COMMA(x) CPP_IFELSE(x, RPC_COMMA, RPC_NOCOMMA)

/* Append the argument __RLA_ARG, whose type is __RLA_TYPE, to the
   message buffer MSG.  */
#define RPCLOADARG(__rla_type, __rla_arg)				\
  {									\
    if (__builtin_strcmp (#__rla_type, "cap_t") == 0)			\
      {									\
	union								\
	{								\
	  __rla_type __rla_a;						\
	  RPC_GRAB2 (, 1, RPC_TYPE_SHIFT (1, struct vg_cap *, cap_t, __rla_foo)); \
	  cap_t __rla_cap;						\
	} __rla_arg2 = { (__rla_arg) };					\
	vg_message_append_cap (msg, __rla_arg2.__rla_cap);		\
      }									\
    else								\
      {									\
	union								\
	{								\
	  __rla_type __rla_a;						\
	  uintptr_t __rla_raw[(sizeof (__rla_type) + sizeof (uintptr_t) - 1) \
			      / sizeof (uintptr_t)];			\
	} __rla_arg2 = { (__rla_arg) };					\
	int __rla_i;							\
	for (__rla_i = 0;						\
	     __rla_i < sizeof (__rla_arg2) / sizeof (uintptr_t);	\
	     __rla_i ++)						\
	  vg_message_append_word (msg, __rla_arg2.__rla_raw[__rla_i]);	\
      }									\
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

/* Store the next argument in the message MSG whose type is __RSA_TYPE
   in *__RSA_ARG.  */
#define RPCSTOREARG(__rsa_type, __rsa_arg)				\
  {									\
    if (__builtin_strcmp (#__rsa_type, "cap_t") == 0)			\
      {									\
	union								\
	{								\
	  __rsa_type *__rsa_a;						\
	  cap_t *__rsa_cap;						\
	} __rsa_arg2;							\
	__rsa_arg2.__rsa_a = __rsa_arg;					\
	if (vg_message_cap_count (msg) > __rsu_cap_idx)			\
	  {								\
	    if (__rsa_arg)						\
	      *__rsa_arg2.__rsa_cap = vg_message_cap (msg, __rsu_cap_idx); \
	    __rsu_cap_idx ++;						\
	  }								\
	else								\
	  __rsu_err = EINVAL;						\
      }									\
    else								\
      {									\
	union								\
	{								\
	  __rsa_type __rsa_a;						\
	  uintptr_t __rsa_raw[(sizeof (__rsa_type) + sizeof (uintptr_t) - 1) \
			      / sizeof (uintptr_t)];			\
	} __rsa_arg2;							\
	int __rsa_i;							\
	for (__rsa_i = 0;						\
	     __rsa_i < sizeof (__rsa_arg2) / sizeof (uintptr_t);	\
	     __rsa_i ++)						\
	  if (vg_message_data_count (msg) / sizeof (uintptr_t)		\
	      > __rsu_data_idx)						\
	    __rsa_arg2.__rsa_raw[__rsa_i]				\
	      = vg_message_word (msg, __rsu_data_idx ++);		\
	  else								\
	    __rsu_err = EINVAL;						\
	if (! __rsu_err && __rsa_arg)					\
	  *(__rsa_arg) = __rsa_arg2.__rsa_a;				\
      }									\
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

/* Marshal a request.  */
#define RPC_SEND_MARSHAL(id, icount, ...)				\
  static inline void							\
  __attribute__((always_inline))					\
  RPC_CONCAT (RPC_STUB_PREFIX_(id), _send_marshal)			\
    (struct vg_message *msg,						\
     RPC_GRAB2 (, icount, ##__VA_ARGS__) RPC_IF_COMMA(icount) ()	\
     cap_t reply_messenger)						\
  {									\
    vg_message_clear (msg);						\
    /* Add the label.  */						\
    vg_message_append_word (msg, RPC_ID_PREFIX_(id));			\
    /* The reply messenger.  */						\
    vg_message_append_cap (msg, reply_messenger);			\
    /* And finally, load the arguments.  */				\
    RPCLOAD (icount, ##__VA_ARGS__);					\
  }

/* Unmarshal a request.  */
#define RPC_SEND_UNMARSHAL(id, icount, ...)				\
  static inline error_t							\
  __attribute__((always_inline))					\
  RPC_CONCAT (RPC_STUB_PREFIX_(id), _send_unmarshal)			\
    (struct vg_message *msg,						\
     RPC_GRAB2 (*, icount, ##__VA_ARGS__) RPC_IF_COMMA(icount) ()	\
     cap_t *reply_messenger)						\
  {									\
    uintptr_t label = 0;						\
    if (likely (vg_message_data_count (msg) >= sizeof (uintptr_t)))	\
      label = vg_message_word (msg, 0);					\
    if (label != RPC_ID_PREFIX_(id))					\
      {									\
	debug (1, #id " has bad method id, %d, excepted %d",		\
	       label, RPC_ID_PREFIX_(id));				\
	return EINVAL;							\
      }									\
    									\
    if (reply_messenger)						\
      *reply_messenger = vg_message_cap (msg, 0);			\
									\
    int __rsu_data_idx __attribute__ ((unused)) = 1;			\
    int __rsu_cap_idx __attribute__ ((unused)) = 1;			\
    error_t __rsu_err = 0;						\
    RPCSTORE (icount, ##__VA_ARGS__);					\
    /* Note: we do not error out if there are too many arguments.  */	\
    if (unlikely (__rsu_err))						\
      {									\
	debug (1, #id " has too little data: "				\
	       "got %d bytes and %d caps; expected %d/%d",		\
	       __rsu_data_idx * sizeof (uintptr_t), __rsu_cap_idx + 1,	\
	       vg_message_data_count (msg),				\
	       vg_message_cap_count (msg));				\
	return EINVAL;							\
      }									\
									\
    return 0;								\
  }

/* Prepare a receive buffer.  */
#ifdef RM_INTERN
#define RPC_RECEIVE_MARSHAL(id, ret_cap_count, ...)
#else
#define RPC_RECEIVE_MARSHAL(id, ret_cap_count, ...)			\
  static inline void							\
  __attribute__((always_inline))					\
  RPC_CONCAT (RPC_STUB_PREFIX_(id), _receive_marshal)			\
    (struct vg_message *msg RPC_IF_COMMA(ret_cap_count) ()		\
     RPC_GRAB2 (, ret_cap_count, ##__VA_ARGS__))			\
  {									\
    vg_message_clear (msg);						\
    /* Load the arguments.  */						\
    RPCLOAD (ret_cap_count, ##__VA_ARGS__);				\
    assert (vg_message_data_count (msg) == 0);				\
    assert (vg_message_cap_count (msg) == ret_cap_count);		\
  }
#endif

/* Marshal a reply.  */
#define RPC_REPLY_MARSHAL(id, out_count, ret_cap_count, ...)		\
  static inline void							\
  __attribute__((always_inline))					\
  RPC_CONCAT (RPC_STUB_PREFIX_(id), _reply_marshal)			\
    (struct vg_message *msg						\
     RPC_IF_COMMA (out_count) ()					\
     RPC_GRAB2 (, out_count, ##__VA_ARGS__)				\
     RPC_IF_COMMA (ret_cap_count) ()					\
     RPC_GRAB2 (, ret_cap_count,					\
		RPC_TYPE_SHIFT (ret_cap_count, struct vg_cap *,		\
				RPC_CHOP2 (out_count, __VA_ARGS__))))	\
  {									\
    vg_message_clear (msg);						\
    									\
    /* The error code.  */						\
    vg_message_append_word (msg, 0);					\
    RPCLOAD (CPP_ADD (out_count, ret_cap_count), ##__VA_ARGS__);	\
									\
    assert (vg_message_cap_count (msg) == ret_cap_count);		\
  }

/* Unmarshal a reply.  */
#define RPC_REPLY_UNMARSHAL(id, out_count, ret_cap_count, ...)		\
  static inline error_t							\
  __attribute__((always_inline))					\
  RPC_CONCAT (RPC_STUB_PREFIX_(id), _reply_unmarshal)			\
    (struct vg_message *msg						\
     RPC_IF_COMMA (CPP_ADD (out_count, ret_cap_count)) ()		\
     RPC_GRAB2(*, CPP_ADD (out_count, ret_cap_count), ##__VA_ARGS__))	\
  {									\
    /* The server error code.  */					\
    error_t __rsu_err = EINVAL;						\
    if (likely (vg_message_data_count (msg) >= sizeof (uintptr_t)))	\
      __rsu_err = vg_message_word (msg, 0);				\
    if (unlikely (__rsu_err))						\
      return __rsu_err;							\
    									\
    int __rsu_data_idx __attribute__ ((unused)) = 1;			\
    int __rsu_cap_idx __attribute__ ((unused)) = 0;			\
    RPCSTORE (CPP_ADD (out_count, ret_cap_count), ##__VA_ARGS__);	\
    /* Note: we do not error out if there are too many arguments.  */	\
    if (unlikely (__rsu_err))						\
      {									\
	debug (1, #id " has too little data: "				\
	       "got %d bytes and %d caps; expected %d/%d",		\
	       __rsu_data_idx * sizeof (uintptr_t), __rsu_cap_idx,	\
	       vg_message_data_count (msg),				\
	       vg_message_cap_count (msg));				\
	return EINVAL;							\
      }									\
    return 0;								\
  }

/* RPC_ARGUMENTS takes a list of types and arguments and returns the first
   COUNT arguments.  (NB: the list may contain more than COUNT
   arguments!).  

     RPC_ARGUMENTS(2, &, int, i, int, j, double, d)

   =>

     &i, &j
*/
#define RPC_ARGUMENTS0(...)
#define RPC_ARGUMENTS1(__ra_prefix, __ra_type, __ra_arg, ...) __ra_prefix __ra_arg RPC_ARGUMENTS0(__ra_prefix, __VA_ARGS__)
#define RPC_ARGUMENTS2(__ra_prefix, __ra_type, __ra_arg, ...) __ra_prefix __ra_arg, RPC_ARGUMENTS1(__ra_prefix, __VA_ARGS__)
#define RPC_ARGUMENTS3(__ra_prefix, __ra_type, __ra_arg, ...) __ra_prefix __ra_arg, RPC_ARGUMENTS2(__ra_prefix, __VA_ARGS__)
#define RPC_ARGUMENTS4(__ra_prefix, __ra_type, __ra_arg, ...) __ra_prefix __ra_arg, RPC_ARGUMENTS3(__ra_prefix, __VA_ARGS__)
#define RPC_ARGUMENTS5(__ra_prefix, __ra_type, __ra_arg, ...) __ra_prefix __ra_arg, RPC_ARGUMENTS4(__ra_prefix, __VA_ARGS__)
#define RPC_ARGUMENTS6(__ra_prefix, __ra_type, __ra_arg, ...) __ra_prefix __ra_arg, RPC_ARGUMENTS5(__ra_prefix, __VA_ARGS__)
#define RPC_ARGUMENTS7(__ra_prefix, __ra_type, __ra_arg, ...) __ra_prefix __ra_arg, RPC_ARGUMENTS6(__ra_prefix, __VA_ARGS__)
#define RPC_ARGUMENTS8(__ra_prefix, __ra_type, __ra_arg, ...) __ra_prefix __ra_arg, RPC_ARGUMENTS7(__ra_prefix, __VA_ARGS__)
#define RPC_ARGUMENTS9(__ra_prefix, __ra_type, __ra_arg, ...) __ra_prefix __ra_arg, RPC_ARGUMENTS8(__ra_prefix, __VA_ARGS__)
#define RPC_ARGUMENTS10(__ra_prefix, __ra_type, __ra_arg, ...) __ra_prefix __ra_arg, RPC_ARGUMENTS9(__ra_prefix, __VA_ARGS__)
#define RPC_ARGUMENTS11(__ra_prefix, __ra_type, __ra_arg, ...) __ra_prefix __ra_arg, RPC_ARGUMENTS10(__ra_prefix, __VA_ARGS__)
#define RPC_ARGUMENTS12(__ra_prefix, __ra_type, __ra_arg, ...) __ra_prefix __ra_arg, RPC_ARGUMENTS11(__ra_prefix, __VA_ARGS__)
#define RPC_ARGUMENTS13(__ra_prefix, __ra_type, __ra_arg, ...) __ra_prefix __ra_arg, RPC_ARGUMENTS12(__ra_prefix, __VA_ARGS__)
#define RPC_ARGUMENTS14(__ra_prefix, __ra_type, __ra_arg, ...) __ra_prefix __ra_arg, RPC_ARGUMENTS13(__ra_prefix, __VA_ARGS__)
#define RPC_ARGUMENTS15(__ra_prefix, __ra_type, __ra_arg, ...) __ra_prefix __ra_arg, RPC_ARGUMENTS14(__ra_prefix, __VA_ARGS__)
#define RPC_ARGUMENTS16(__ra_prefix, __ra_type, __ra_arg, ...) __ra_prefix __ra_arg, RPC_ARGUMENTS15(__ra_prefix, __VA_ARGS__)
#define RPC_ARGUMENTS17(__ra_prefix, __ra_type, __ra_arg, ...) __ra_prefix __ra_arg, RPC_ARGUMENTS16(__ra_prefix, __VA_ARGS__)
#define RPC_ARGUMENTS18(__ra_prefix, __ra_type, __ra_arg, ...) __ra_prefix __ra_arg, RPC_ARGUMENTS17(__ra_prefix, __VA_ARGS__)
#define RPC_ARGUMENTS19(__ra_prefix, __ra_type, __ra_arg, ...) __ra_prefix __ra_arg, RPC_ARGUMENTS18(__ra_prefix, __VA_ARGS__)
#define RPC_ARGUMENTS20(__ra_prefix, __ra_type, __ra_arg, ...) __ra_prefix __ra_arg, RPC_ARGUMENTS19(__ra_prefix, __VA_ARGS__)
#define RPC_ARGUMENTS_(__ra_count, __ra_prefix, ...) RPC_ARGUMENTS##__ra_count(__ra_prefix, __VA_ARGS__)
#define RPC_ARGUMENTS(__ra_count, __ra_prefix, ...) RPC_ARGUMENTS_(__ra_count, __ra_prefix, __VA_ARGS__)

/* Given a list of arguments, returns the arguments minus the first
   COUNT **pairs** of arguments.  For example:

     RPC_CHOP2(1, int, i, int, j, double, d)

   =>

     int, j, double, d

  */
#define RPC_CHOP2_0(...) __VA_ARGS__
#define RPC_CHOP2_1(__rc_a, __rc_b, ...) RPC_CHOP2_0(__VA_ARGS__)
#define RPC_CHOP2_2(__rc_a, __rc_b, ...) RPC_CHOP2_1(__VA_ARGS__)
#define RPC_CHOP2_3(__rc_a, __rc_b, ...) RPC_CHOP2_2(__VA_ARGS__)
#define RPC_CHOP2_4(__rc_a, __rc_b, ...) RPC_CHOP2_3(__VA_ARGS__)
#define RPC_CHOP2_5(__rc_a, __rc_b, ...) RPC_CHOP2_4(__VA_ARGS__)
#define RPC_CHOP2_6(__rc_a, __rc_b, ...) RPC_CHOP2_5(__VA_ARGS__)
#define RPC_CHOP2_7(__rc_a, __rc_b, ...) RPC_CHOP2_6(__VA_ARGS__)
#define RPC_CHOP2_8(__rc_a, __rc_b, ...) RPC_CHOP2_7(__VA_ARGS__)
#define RPC_CHOP2_9(__rc_a, __rc_b, ...) RPC_CHOP2_8(__VA_ARGS__)
#define RPC_CHOP2_10(__rc_a, __rc_b, ...) RPC_CHOP2_9(__VA_ARGS__)
#define RPC_CHOP2_11(__rc_a, __rc_b, ...) RPC_CHOP2_10(__VA_ARGS__)
#define RPC_CHOP2_12(__rc_a, __rc_b, ...) RPC_CHOP2_11(__VA_ARGS__)
#define RPC_CHOP2_13(__rc_a, __rc_b, ...) RPC_CHOP2_12(__VA_ARGS__)
#define RPC_CHOP2_14(__rc_a, __rc_b, ...) RPC_CHOP2_13(__VA_ARGS__)
#define RPC_CHOP2_15(__rc_a, __rc_b, ...) RPC_CHOP2_14(__VA_ARGS__)
#define RPC_CHOP2_16(__rc_a, __rc_b, ...) RPC_CHOP2_15(__VA_ARGS__)
#define RPC_CHOP2_17(__rc_a, __rc_b, ...) RPC_CHOP2_16(__VA_ARGS__)
#define RPC_CHOP2_18(__rc_a, __rc_b, ...) RPC_CHOP2_17(__VA_ARGS__)
#define RPC_CHOP2_19(__rc_a, __rc_b, ...) RPC_CHOP2_18(__VA_ARGS__)
#define RPC_CHOP2_20(__rc_a, __rc_b, ...) RPC_CHOP2_19(__VA_ARGS__)
#define RPC_CHOP2_21(__rc_a, __rc_b, ...) RPC_CHOP2_20(__VA_ARGS__)
#define RPC_CHOP2_22(__rc_a, __rc_b, ...) RPC_CHOP2_21(__VA_ARGS__)
#define RPC_CHOP2_23(__rc_a, __rc_b, ...) RPC_CHOP2_22(__VA_ARGS__)
#define RPC_CHOP2_24(__rc_a, __rc_b, ...) RPC_CHOP2_23(__VA_ARGS__)
#define RPC_CHOP2_25(__rc_a, __rc_b, ...) RPC_CHOP2_24(__VA_ARGS__)
#define RPC_CHOP2_(__rc_count, ...) RPC_CHOP2_##__rc_count (__VA_ARGS__)
#define RPC_CHOP2(__rc_count, ...) RPC_CHOP2_(__rc_count, __VA_ARGS__)

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

#define RPC_TYPE_SHIFT_0(...)
#define RPC_TYPE_SHIFT_1(__ra_new_type, __ra_type, __ra_arg, ...) __ra_new_type, __ra_arg RPC_TYPE_SHIFT_0(__ra_new_type, __VA_ARGS__)
#define RPC_TYPE_SHIFT_2(__ra_new_type, __ra_type, __ra_arg, ...) __ra_new_type, __ra_arg, RPC_TYPE_SHIFT_1(__ra_new_type, __VA_ARGS__)
#define RPC_TYPE_SHIFT_3(__ra_new_type, __ra_type, __ra_arg, ...) __ra_new_type, __ra_arg, RPC_TYPE_SHIFT_2(__ra_new_type, __VA_ARGS__)
#define RPC_TYPE_SHIFT_4(__ra_new_type, __ra_type, __ra_arg, ...) __ra_new_type, __ra_arg, RPC_TYPE_SHIFT_3(__ra_new_type, __VA_ARGS__)
#define RPC_TYPE_SHIFT_5(__ra_new_type, __ra_type, __ra_arg, ...) __ra_new_type, __ra_arg, RPC_TYPE_SHIFT_4(__ra_new_type, __VA_ARGS__)
#define RPC_TYPE_SHIFT_6(__ra_new_type, __ra_type, __ra_arg, ...) __ra_new_type, __ra_arg, RPC_TYPE_SHIFT_5(__ra_new_type, __VA_ARGS__)
#define RPC_TYPE_SHIFT_7(__ra_new_type, __ra_type, __ra_arg, ...) __ra_new_type, __ra_arg, RPC_TYPE_SHIFT_6(__ra_new_type, __VA_ARGS__)
#define RPC_TYPE_SHIFT_8(__ra_new_type, __ra_type, __ra_arg, ...) __ra_new_type, __ra_arg, RPC_TYPE_SHIFT_7(__ra_new_type, __VA_ARGS__)
#define RPC_TYPE_SHIFT_9(__ra_new_type, __ra_type, __ra_arg, ...) __ra_new_type, __ra_arg, RPC_TYPE_SHIFT_8(__ra_new_type, __VA_ARGS__)
#define RPC_TYPE_SHIFT_10(__ra_new_type, __ra_type, __ra_arg, ...) __ra_new_type, __ra_arg, RPC_TYPE_SHIFT_9(__ra_new_type, __VA_ARGS__)
#define RPC_TYPE_SHIFT_11(__ra_new_type, __ra_type, __ra_arg, ...) __ra_new_type, __ra_arg, RPC_TYPE_SHIFT_10(__ra_new_type, __VA_ARGS__)
#define RPC_TYPE_SHIFT_12(__ra_new_type, __ra_type, __ra_arg, ...) __ra_new_type, __ra_arg, RPC_TYPE_SHIFT_11(__ra_new_type, __VA_ARGS__)
#define RPC_TYPE_SHIFT_13(__ra_new_type, __ra_type, __ra_arg, ...) __ra_new_type, __ra_arg, RPC_TYPE_SHIFT_12(__ra_new_type, __VA_ARGS__)
#define RPC_TYPE_SHIFT_14(__ra_new_type, __ra_type, __ra_arg, ...) __ra_new_type, __ra_arg, RPC_TYPE_SHIFT_13(__ra_new_type, __VA_ARGS__)
#define RPC_TYPE_SHIFT_15(__ra_new_type, __ra_type, __ra_arg, ...) __ra_new_type, __ra_arg, RPC_TYPE_SHIFT_14(__ra_new_type, __VA_ARGS__)
#define RPC_TYPE_SHIFT_16(__ra_new_type, __ra_type, __ra_arg, ...) __ra_new_type, __ra_arg, RPC_TYPE_SHIFT_15(__ra_new_type, __VA_ARGS__)
#define RPC_TYPE_SHIFT_17(__ra_new_type, __ra_type, __ra_arg, ...) __ra_new_type, __ra_arg, RPC_TYPE_SHIFT_16(__ra_new_type, __VA_ARGS__)
#define RPC_TYPE_SHIFT_18(__ra_new_type, __ra_type, __ra_arg, ...) __ra_new_type, __ra_arg, RPC_TYPE_SHIFT_17(__ra_new_type, __VA_ARGS__)
#define RPC_TYPE_SHIFT_19(__ra_new_type, __ra_type, __ra_arg, ...) __ra_new_type, __ra_arg, RPC_TYPE_SHIFT_18(__ra_new_type, __VA_ARGS__)
#define RPC_TYPE_SHIFT_20(__ra_new_type, __ra_type, __ra_arg, ...) __ra_new_type, __ra_arg, RPC_TYPE_SHIFT_19(__ra_new_type, __VA_ARGS__)
#define RPC_TYPE_SHIFT_(__ra_count, __ra_new_type, ...) RPC_TYPE_SHIFT_##__ra_count(__ra_new_type, __VA_ARGS__)
#ifdef RM_INTERN
# define RPC_TYPE_SHIFT(__ra_count, __ra_new_type, ...) RPC_TYPE_SHIFT_(__ra_count, __ra_new_type, __VA_ARGS__)
#else
# define RPC_TYPE_SHIFT(__ra_count, __ra_new_type, ...) __VA_ARGS__
#endif

/* Ensure that there are X pairs of arguments.  */
#define RPC_INVALID_NUMBER_OF_ARGUMENTS_
#define RPC_EMPTY_LIST_(x) RPC_INVALID_NUMBER_OF_ARGUMENTS_##x
#define RPC_EMPTY_LIST(x) RPC_EMPTY_LIST_(x)
#define RPC_ENSURE_ARGS(count, ...) \
  RPC_EMPTY_LIST (RPC_CHOP2 (count, __VA_ARGS__))

#define RPC_SEND_MARSHAL_GEN_(id, in_count, out_count, ret_cap_count, ...) \
  RPC_SEND_MARSHAL(id, in_count, ##__VA_ARGS__)				\
  RPC_SEND_UNMARSHAL(id, in_count, ##__VA_ARGS__)

#define RPC_RECEIVE_MARSHAL_GEN_(id, in_count, out_count, ret_cap_count, ...) \
  RPC_RECEIVE_MARSHAL(id, ret_cap_count,				\
		      RPC_CHOP2 (CPP_ADD (in_count, out_count),		\
				 ##__VA_ARGS__))

#define RPC_REPLY_MARSHAL_GEN_(id, in_count, out_count, ret_cap_count, ...) \
  RPC_REPLY_MARSHAL(id, out_count, ret_cap_count,			\
		    RPC_CHOP2 (in_count, ##__VA_ARGS__))		\
  RPC_REPLY_UNMARSHAL(id, out_count, ret_cap_count,			\
		      RPC_CHOP2 (in_count, ##__VA_ARGS__))

#define RPC_MARSHAL_GEN_(id, in_count, out_count, ret_cap_count, ...)	\
  RPC_ENSURE_ARGS(CPP_ADD (CPP_ADD (in_count, out_count),		\
			   ret_cap_count),				\
		  ##__VA_ARGS__)					\
  RPC_SEND_MARSHAL_GEN_(id, in_count, out_count, ret_cap_count,		\
			##__VA_ARGS__)					\
  RPC_RECEIVE_MARSHAL_GEN_(id, in_count, out_count, ret_cap_count,	\
			   ##__VA_ARGS__)				\
  RPC_REPLY_MARSHAL_GEN_(id, in_count, out_count, ret_cap_count,	\
			 ##__VA_ARGS__)

/* Send a message.  __RPC_REPY_MESSENGER designates the messenger that
   should receive the reply.  (Its buffer should have already been
   prepared using, e.g., the corresponding receive_marshal
   function.)  */
#ifndef RM_INTERN
#define RPC_SEND_(postfix, id, in_count, out_count, ret_cap_count, ...) \
  static inline error_t							\
  __attribute__((always_inline))					\
  RPC_CONCAT(RPC_STUB_PREFIX_(id), postfix)				\
    (cap_t __rpc_activity, cap_t __rpc_object				\
     RPC_IF_COMMA (in_count) ()						\
     RPC_GRAB2 (, in_count, __VA_ARGS__),				\
     cap_t __rpc_reply_messenger)					\
  {									\
    struct hurd_message_buffer *mb = hurd_message_buffer_alloc ();	\
    mb->just_free = true;						\
									\
    RPC_CONCAT (RPC_STUB_PREFIX_(id), _send_marshal)			\
      (mb->request							\
       RPC_IF_COMMA (in_count) () RPC_ARGUMENTS(in_count,, __VA_ARGS__), \
       __rpc_reply_messenger);						\
									\
    error_t err = vg_send (VG_IPC_SEND_SET_THREAD_TO_CALLER		\
			   | VG_IPC_SEND_SET_ASROOT_TO_CALLERS,		\
			   __rpc_activity, __rpc_object,		\
			   mb->sender, VG_ADDR_VOID);			\
									\
    return err;								\
  }
#else
#define RPC_SEND_(postfix, id, in_count, out_count, ret_cap_count, ...)
#endif

/* Send a message.  Abort if the target is not ready.  */
#ifndef RM_INTERN
#define RPC_SEND_NONBLOCKING_(postfix, id, in_count, out_count, ret_cap_count, ...) \
  static inline error_t							\
  __attribute__((always_inline))					\
  RPC_CONCAT(RPC_STUB_PREFIX_(id), postfix)				\
    (cap_t __rpc_activity, cap_t __rpc_object				\
     RPC_IF_COMMA (in_count) ()						\
     RPC_GRAB2 (, in_count, __VA_ARGS__),				\
     cap_t __rpc_reply_messenger)					\
  {									\
    struct hurd_message_buffer *mb = hurd_message_buffer_alloc ();	\
									\
    RPC_CONCAT (RPC_STUB_PREFIX_(id), _send_marshal)			\
      (mb->request							\
       RPC_IF_COMMA (in_count) () RPC_ARGUMENTS(in_count,, __VA_ARGS__), \
       __rpc_reply_messenger);						\
									\
    error_t err = vg_reply (VG_IPC_SEND_SET_THREAD_TO_CALLER		\
			    | VG_IPC_SEND_SET_ASROOT_TO_CALLERS,	\
			    __rpc_activity, __rpc_object,		\
			    mb->sender, VG_ADDR_VOID);			\
									\
    hurd_message_buffer_free (mb);					\
									\
    return err;								\
  }
#else
#define RPC_SEND_NONBLOCKING_(postfix, id, in_count, out_count, ret_cap_count, ...)
#endif

/* Send a message and wait for a reply.  */
#ifndef RM_INTERN
#define RPC_(postfix, id, in_count, out_count, ret_cap_count, ...)	\
  static inline error_t							\
  __attribute__((always_inline))					\
  RPC_CONCAT (RPC_CONCAT (RPC_STUB_PREFIX_(id), _using), postfix)	\
    (struct hurd_message_buffer *mb,					\
     vg_addr_t __rpc_activity,						\
     vg_addr_t __rpc_object						\
     /* In arguments.  */						\
     RPC_IF_COMMA (in_count) ()						\
     RPC_GRAB2 (, in_count, __VA_ARGS__)				\
     /* Out arguments (data and caps).  */				\
     RPC_IF_COMMA (CPP_ADD (out_count, ret_cap_count)) ()		\
     RPC_GRAB2 (*, CPP_ADD (out_count, ret_cap_count),			\
		RPC_CHOP2 (in_count, __VA_ARGS__)))			\
  {									\
    /* Prepare the reply buffer.  */					\
    RPC_CONCAT (RPC_STUB_PREFIX_(id), _receive_marshal)			\
      (mb->reply							\
       RPC_IF_COMMA (ret_cap_count) ()					\
       CPP_FOREACH(ret_cap_count, CPP_SAFE_DEREF, VG_ADDR_VOID,		\
		   RPC_ARGUMENTS (ret_cap_count, ,			\
				  RPC_CHOP2 (CPP_ADD (in_count, out_count), \
					     __VA_ARGS__))));		\
									\
    /* Then the send buffer.  */					\
    RPC_CONCAT (RPC_STUB_PREFIX_(id), _send_marshal)			\
      (mb->request							\
       RPC_IF_COMMA (in_count) ()					\
       RPC_ARGUMENTS (in_count,, __VA_ARGS__),				\
       mb->receiver);							\
									\
    hurd_activation_message_register (mb);				\
									\
    /* We will be resumed via an activation.  */			\
    error_t err = vg_ipc (VG_IPC_RECEIVE | VG_IPC_SEND			\
			  | VG_IPC_RECEIVE_ACTIVATE			\
			  | VG_IPC_SEND_SET_THREAD_TO_CALLER		\
			  | VG_IPC_SEND_SET_ASROOT_TO_CALLERS		\
			  | VG_IPC_RECEIVE_SET_THREAD_TO_CALLER		\
			  | VG_IPC_RECEIVE_SET_ASROOT_TO_CALLERS,	\
			  __rpc_activity,				\
			  mb->receiver_strong, VG_ADDR_VOID,		\
			  __rpc_activity, __rpc_object,			\
			  mb->sender, VG_ADDR_VOID);			\
    if (err)								\
      /* Error sending the IPC.  */					\
      hurd_activation_message_unregister (mb);				\
    else								\
      err = RPC_CONCAT (RPC_STUB_PREFIX_(id), _reply_unmarshal)		\
	(mb->reply							\
	 RPC_IF_COMMA (CPP_ADD (out_count, ret_cap_count)) ()		\
	 RPC_ARGUMENTS (CPP_ADD (out_count, ret_cap_count),,		\
			RPC_CHOP2 (in_count, ##__VA_ARGS__)));		\
									\
    return err;								\
  }									\
									\
  static inline error_t							\
  __attribute__((always_inline))					\
  RPC_CONCAT (RPC_STUB_PREFIX_(id), postfix)				\
    (vg_addr_t __rpc_activity,						\
     vg_addr_t __rpc_object						\
     /* In arguments.  */						\
     RPC_IF_COMMA (in_count) ()						\
     RPC_GRAB2 (, in_count, __VA_ARGS__)				\
     /* Out arguments (data and caps).  */				\
     RPC_IF_COMMA (CPP_ADD (out_count, ret_cap_count)) ()		\
     RPC_GRAB2 (*, CPP_ADD (out_count, ret_cap_count),			\
		RPC_CHOP2 (in_count, __VA_ARGS__)))			\
  {									\
    struct hurd_message_buffer *mb = hurd_message_buffer_alloc ();	\
									\
    error_t err;							\
    err = RPC_CONCAT (RPC_CONCAT (RPC_STUB_PREFIX_(id), _using), postfix) \
      (mb, __rpc_activity, __rpc_object					\
       RPC_IF_COMMA (CPP_ADD (CPP_ADD (in_count, out_count),		\
			      ret_cap_count)) ()			\
       RPC_ARGUMENTS (CPP_ADD (CPP_ADD (in_count, out_count),		\
			       ret_cap_count),, __VA_ARGS__));		\
									\
    hurd_message_buffer_free (mb);					\
									\
    return err;								\
  }
#else
# define RPC_(postfix, id, in_count, out_count, ret_cap_count, ...)
#endif

/* Send a reply to __RPC_TARGET.  If __RPC_TARGET does not accept the
   message immediately, abort sending.  */
#ifndef RM_INTERN
#define RPC_REPLY_(id, in_count, out_count, ret_cap_count, ...)		\
  static inline error_t							\
  RPC_CONCAT (RPC_STUB_PREFIX_(id), _reply)				\
    (vg_addr_t __rpc_activity,						\
     vg_addr_t __rpc_target						\
     /* Out data.  */							\
     RPC_IF_COMMA (out_count) ()					\
     RPC_GRAB2 (, out_count, RPC_CHOP2 (in_count, ##__VA_ARGS__))	\
     /* Return capabilities.  */					\
     RPC_IF_COMMA (ret_cap_count) ()					\
     RPC_GRAB2 (, ret_cap_count,					\
		RPC_CHOP2 (CPP_ADD (in_count, out_count),		\
			   ##__VA_ARGS__)))				\
  {									\
    struct hurd_message_buffer *mb = hurd_message_buffer_alloc ();	\
									\
    RPC_CONCAT (RPC_STUB_PREFIX_(id), _reply_marshal)			\
      (mb->request							\
       /* Out data.  */							\
       RPC_IF_COMMA (out_count) ()					\
       RPC_ARGUMENTS(out_count,, RPC_CHOP2 (in_count, __VA_ARGS__))	\
       /* Out capabilities.  */						\
       RPC_IF_COMMA (ret_cap_count) ()					\
       RPC_ARGUMENTS(ret_cap_count,,					\
		     RPC_CHOP2 (CPP_ADD (in_count, out_count),		\
				__VA_ARGS__)));				\
									\
    error_t err = vg_reply (VG_IPC_SEND_SET_THREAD_TO_CALLER		\
			    | VG_IPC_SEND_SET_ASROOT_TO_CALLERS,	\
			    __rpc_activity, __rpc_target,		\
			    mb->sender, VG_ADDR_VOID);			\
									\
    hurd_message_buffer_free (mb);					\
									\
    return err;								\
  }
#else
#define RPC_REPLY_(id, in_count, out_count, ret_cap_count, ...)		\
  static inline error_t							\
  __attribute__((always_inline))					\
  RPC_CONCAT (RPC_STUB_PREFIX_(id), _reply)				\
    (struct activity *__rpc_activity,					\
     struct messenger *__rpc_target					\
     /* Out data.  */							\
     RPC_IF_COMMA (out_count) ()					\
     RPC_GRAB2 (, out_count, RPC_CHOP2 (in_count, ##__VA_ARGS__))	\
     /* Return capabilities.  */					\
     RPC_IF_COMMA (ret_cap_count) ()					\
     RPC_GRAB2 (, ret_cap_count,					\
		RPC_TYPE_SHIFT (ret_cap_count, struct vg_cap,		\
				RPC_CHOP2 (CPP_ADD (in_count, out_count), \
					   ##__VA_ARGS__))))		\
  {									\
    RPC_CONCAT (RPC_STUB_PREFIX_(id), _reply_marshal)			\
      (reply_buffer							\
       /* Out data.  */							\
       RPC_IF_COMMA (out_count) ()					\
       RPC_ARGUMENTS(out_count,, RPC_CHOP2 (in_count, ##__VA_ARGS__))	\
       /* Out capabilities.  */						\
       RPC_IF_COMMA (ret_cap_count) ()					\
       RPC_ARGUMENTS (ret_cap_count, &,					\
		      RPC_CHOP2 (CPP_ADD (in_count, out_count),		\
				 ##__VA_ARGS__)));			\
									\
    bool ret = messenger_message_load (__rpc_activity,			\
				       __rpc_target, reply_buffer);	\
									\
    return ret ? 0 : EWOULDBLOCK;					\
  }
#endif

/* RPC template.  ID is the method name.  IN_COUNT is the number of
   arguments.  OUT_COUNT is the number of out arguments.
   RET_CAP_COUNT is the number of capabilities that are returned.  The
   remaining arguments correspond to pairs of types and argument
   names.

   Consider:

     RPC(method, 2, 1, 1,
         // In (data and capability) parameters
         int, foo, cap_t, bar,
	 // Out data parameters
         int bam,
	 // Out capabilities
	 cap_t xyzzy)

   This will generate marshalling and unmarshalling functions as well
   as send, reply and call functions.  For instance, the signature for
   the correspond send marshal function is:

     error_t method_send_marshal (struct vg_message *message,
                                  int foo, cap_t bar, cap_t reply)

   that of the send unmarshal function is:
     
     error_t method_send_unmarshal (struct vg_message *message,
                                    int *foo, cap_t *bar, cap_t *reply)

   that of the receive marshal function is:

     error_t method_receive_marshal (struct vg_message *message,
                                     cap_t xyzzy)


   that of the reply marshal function is:

     error_t method_reply_marshal (struct vg_message *message,
                                   int bam, cap_t xyzzy)

   that of the reply unmarshal function is:

     error_t method_reply_unmarshal (struct vg_message *message,
                                     int *bam, cap_t *xyzzy)

   Functions to send requests and replies as well as to produce calls
   are also generated.

     error_t method_call (cap_t activity, cap_t object,
                          int foo, cap_t bar, int *bam, cap_t *xyzzy)

   Note that *XYZZY must be initialize with the location of a
   capability slot to store the returned capability.  *XYZZY is set to
   VG_ADDR_VOID if the sender did not provide a capability.

   To send a message and not wait for a reply, a function with the
   following prototype is generated:

     error_t method_send (cap_t activity, cap_t object,
                          int foo, cap_t bar,
			  cap_t reply_messenger)

   To reply to a request, a function with the following prototype is
   generated:

     error_t method_reply (cap_t activity, cap_t reply_messenger,
                           int bam, cap_t xyzzy)
*/

#define RPC(id, in_count, out_count, ret_cap_count, ...)		\
  RPC_MARSHAL_GEN_(id, in_count, out_count, ret_cap_count, ##__VA_ARGS__) \
									\
  RPC_(, id, in_count, out_count, ret_cap_count, ##__VA_ARGS__)		\
  RPC_SEND_(_send, id, in_count, out_count, ret_cap_count,		\
	    ##__VA_ARGS__)						\
  RPC_SEND_NONBLOCKING_(_send_nonblocking,				\
			id, in_count, out_count, ret_cap_count,		\
			##__VA_ARGS__)					\
  RPC_REPLY_(id, in_count, out_count, ret_cap_count, ##__VA_ARGS__)

/* Marshal a reply consisting of the error code ERR in *MSG.  */
static inline void
__attribute__((always_inline))
rpc_error_reply_marshal (struct vg_message *msg, error_t err)
{
  vg_message_clear (msg);
  vg_message_append_word (msg, err);
}

/* Reply to the target TARGET with error code ERROR.  */
#ifdef RM_INTERN
static inline error_t
__attribute__((always_inline))
rpc_error_reply (struct activity *activity, struct messenger *target,
		 error_t err)
{
  rpc_error_reply_marshal (reply_buffer, err);
  bool ret = messenger_message_load (activity, target, reply_buffer);
  return ret ? 0 : EWOULDBLOCK;
}
#else
static inline error_t
__attribute__((always_inline))
rpc_error_reply (cap_t activity, cap_t target, error_t err)
{
  return vg_ipc_short (VG_IPC_SEND_NONBLOCKING | VG_IPC_SEND_INLINE
		       | VG_IPC_SEND_INLINE_WORD1,
		       VG_ADDR_VOID, VG_ADDR_VOID, VG_ADDR_VOID,
		       VG_ADDR_VOID, target,
		       VG_ADDR_VOID, err, 0, VG_ADDR_VOID);
}
#endif

#endif
