/* vregs.h - L4 virtual registers for powerpc.
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

#ifndef _L4_VREGS_H
# error "Never use <l4/bits/vregs.h> directly; include <l4/vregs.h> instead."
#endif


/* Retrieve the UTCB address.  */
static inline l4_word_t *
__attribute__((__always_inline__, __const__))
__l4_utcb (void)
{
  l4_word_t *utcb;

  __asm__ ("mr %[utcb], %%r2"
	   : [utcb] "=r" (utcb));

  return utcb;
}

/* Offsets of various elements in the UTCB, relativ to the UTCB
   address.  */
#define __L4_UTCB_MR0		0
#define __L4_UTCB_THREAD_WORD0	-4
#define __L4_UTCB_THREAD_WORD1	-5
#define __L4_UTCB_SENDER	-6
#define __L4_UTCB_RECEIVER	-7
#define __L4_UTCB_TIMEOUT	-8
#define __L4_UTCB_ERROR_CODE	-9
#define __L4_UTCB_FLAGS		-10
#define __L4_UTCB_EXC_HANDLER	-11
#define __L4_UTCB_PAGER		-12
#define __L4_UTCB_USER_HANDLE	-13
#define __L4_UTCB_PROCESSOR_NO	-14
#define __L4_UTCB_MY_GLOBAL_ID	-15
#define __L4_UTCB_BR0		-16


/* Get the local thread ID.  */
static inline l4_thread_id_t
__attribute__((__always_inline__))
l4_my_local_id (void)
{
  l4_thread_id_t id;

  /* Local thread ID is equal to the UTCB address.  */
  id.raw = (l4_word_t) __l4_utcb ();

  return id;
}


/* Get the global thread ID.  */
static inline l4_thread_id_t
__attribute__((__always_inline__))
l4_my_global_id (void)
{
  l4_thread_id_t id;
  l4_word_t *utcb = __l4_utcb ();

  id.raw = utcb[__L4_UTCB_MY_GLOBAL_ID];
  return id;
}


static inline int
__attribute__((__always_inline__))
l4_processor_no (void)
{
  l4_word_t *utcb = __l4_utcb ();

  return utcb[__L4_UTCB_PROCESSOR_NO];
}


static inline l4_word_t
__attribute__((__always_inline__))
l4_user_defined_handle (void)
{
  l4_word_t *utcb = __l4_utcb ();

  return utcb[__L4_UTCB_USER_HANDLE];
}


static inline void
__attribute__((__always_inline__))
l4_set_user_defined_handle (l4_word_t data)
{
  l4_word_t *utcb = __l4_utcb ();

  utcb[__L4_UTCB_USER_HANDLE] = data;
}


static inline l4_thread_id_t
__attribute__((__always_inline__))
l4_pager (void)
{
  l4_thread_id_t thread;
  l4_word_t *utcb = __l4_utcb ();

  thread.raw = utcb[__L4_UTCB_PAGER];
  return thread;
}


static inline void
__attribute__((__always_inline__))
l4_set_pager (l4_thread_id_t thread)
{
  l4_word_t *utcb = __l4_utcb ();

  utcb[__L4_UTCB_PAGER] = thread.raw;
}


static inline l4_thread_id_t
__attribute__((__always_inline__))
l4_exception_handler (void)
{
  l4_thread_id_t thread;
  l4_word_t *utcb = __l4_utcb ();

  thread.raw = utcb[__L4_UTCB_EXC_HANDLER];
  return thread;
}


static inline void
__attribute__((__always_inline__))
l4_set_exception_handler (l4_thread_id_t thread)
{
  l4_word_t *utcb = __l4_utcb ();

  utcb[__L4_UTCB_EXC_HANDLER] = thread.raw;
}


static inline void
__attribute__((__always_inline__))
l4_clr_cop_flag (l4_word_t n)
{
  l4_word_t *utcb = __l4_utcb ();

  /* The second byte in the flags field contains the coprocessor
     flags.  */
  utcb[__L4_UTCB_FLAGS] &= ~(1 << (8 + n));
}


static inline void
__attribute__((__always_inline__))
l4_set_cop_flag (l4_word_t n)
{
  l4_word_t *utcb = __l4_utcb ();

  /* The second byte in the flags field contains the coprocessor
     flags.  */
  utcb[__L4_UTCB_FLAGS] |= 1 << (8 + n);
}


static inline l4_word_t
__attribute__((__always_inline__))
l4_disable_preemption_fault_exception (void)
{
  l4_word_t *utcb = __l4_utcb ();
  l4_word_t result;

  /* The first byte in the flags field contains the preemption
     flags.  The sixth bit is the signal bit.  */
  result = (utcb[__L4_UTCB_FLAGS] >> 5) & 1;
  utcb[__L4_UTCB_FLAGS] &= ~(1 << 5);

  return result;
}


static inline l4_word_t
__attribute__((__always_inline__))
l4_enable_preemption_fault_exception (void)
{
  l4_word_t *utcb = __l4_utcb ();
  l4_word_t result;

  /* The first byte in the flags field contains the preemption
     flags.  The sixth bit is the signal flag.  */
  result = (utcb[__L4_UTCB_FLAGS] >> 5) & 1;
  utcb[__L4_UTCB_FLAGS] |= 1 << 5;

  return result;
}


static inline l4_word_t
__attribute__((__always_inline__))
l4_disable_preemption (void)
{
  l4_word_t *utcb = __l4_utcb ();
  l4_word_t result;

  /* The first byte in the flags field contains the preemption
     flags.  The seventh bit is the delay flag.  */
  result = (utcb[__L4_UTCB_FLAGS] >> 6) & 1;
  utcb[__L4_UTCB_FLAGS] &= ~(1 << 6);

  return result;
}


static inline l4_word_t
__attribute__((__always_inline__))
l4_enable_preemption (void)
{
  l4_word_t *utcb = __l4_utcb ();
  l4_word_t result;

  /* The first byte in the flags field contains the preemption
     flags.  The seventh bit is the delay flag.  */
  result = (utcb[__L4_UTCB_FLAGS] >> 6) & 1;
  utcb[__L4_UTCB_FLAGS] |= 1 << 6;

  return result;
}


static inline l4_word_t
__attribute__((__always_inline__))
l4_preemption_pending (void)
{
  l4_word_t *utcb = __l4_utcb ();
  l4_word_t result;

  result = (utcb[__L4_UTCB_FLAGS] >> 7) & 1;
  utcb[__L4_UTCB_FLAGS] &= ~(1 << 7);

  return result;
}


static inline l4_word_t
__attribute__((__always_inline__))
l4_error_code (void)
{
  l4_word_t *utcb = __l4_utcb ();

  return utcb[__L4_UTCB_ERROR_CODE];
}


static inline l4_word_t
__attribute__((__always_inline__))
l4_xfer_timeout (void)
{
  l4_word_t *utcb = __l4_utcb ();

  return utcb[__L4_UTCB_TIMEOUT];
}


static inline void
__attribute__((__always_inline__))
l4_set_xfer_timeout (l4_word_t time)
{
  l4_word_t *utcb = __l4_utcb ();

  utcb[__L4_UTCB_TIMEOUT] = time;
}


static inline l4_thread_id_t
__attribute__((__always_inline__))
l4_intended_receiver (void)
{
  l4_thread_id_t thread;
  l4_word_t *utcb = __l4_utcb ();

  thread.raw = utcb[__L4_UTCB_RECEIVER];
  return thread;
}


static inline l4_thread_id_t
__attribute__((__always_inline__))
l4_actual_sender (void)
{
  l4_thread_id_t thread;
  l4_word_t *utcb = __l4_utcb ();

  thread.raw = utcb[__L4_UTCB_SENDER];
  return thread;
}


static inline void
__attribute__((__always_inline__))
l4_set_virtual_sender (l4_thread_id_t thread)
{
  l4_word_t *utcb = __l4_utcb ();

  utcb[__L4_UTCB_SENDER] = thread.raw;
}


/* Message registers (MR0 to MR63) start at offset __L4_UTCB_MR0 and
   go upward.  */

/* Set message register NR to DATA.  */
static inline void
__attribute__((__always_inline__))
l4_load_mr (int nr, l4_word_t data)
{
  l4_word_t *mr = __l4_utcb () + __L4_UTCB_MR0;

  mr[nr] = data;
}


/* Set COUNT message registers beginning from START to the values in
   DATA.  */
static inline void
__attribute__((__always_inline__))
l4_load_mrs (int start, int count, l4_word_t *data)
{
  l4_word_t *mr = __l4_utcb () + __L4_UTCB_MR0 + start;

  while (count--)
    *(mr++) = *(data++);
}


/* Store message register NR in DATA.  */
static inline void
__attribute__((__always_inline__))
l4_store_mr (int nr, l4_word_t *data)
{
  l4_word_t *mr = __l4_utcb () + __L4_UTCB_MR0;

  *data = mr[nr];
}


/* Store COUNT message registers beginning from START in DATA.  */
static inline void
__attribute__((__always_inline__))
l4_store_mrs (int start, int count, l4_word_t *data)
{
  l4_word_t *mr = __l4_utcb () + __L4_UTCB_MR0 + start;

  while (count--)
    *(data++) = *(mr++);
}


/* Buffer registers (BR0 to BR31) start at offset __L4_UTCB_BR0 and go
   downward.  */

/* Set message register NR to DATA.  */
static inline void
__attribute__((__always_inline__))
l4_load_br (int nr, l4_word_t data)
{
  l4_word_t *br = __l4_utcb () + __L4_UTCB_BR0;

  br[nr] = data;
}


/* Set COUNT message registers beginning from START to the values in
   DATA.  */
static inline void
__attribute__((__always_inline__))
l4_load_brs (int start, int count, l4_word_t *data)
{
  l4_word_t *br = __l4_utcb () + __L4_UTCB_BR0 - start;

  while (count--)
    *(br--) = *(data--);
}


/* Store message register NR in DATA.  */
static inline void
__attribute__((__always_inline__))
l4_store_br (int nr, l4_word_t *data)
{
  l4_word_t *br = __l4_utcb () + __L4_UTCB_BR0;

  *data = br[nr];
}


/* Store COUNT message registers beginning from START in DATA.  */
static inline void
__attribute__((__always_inline__))
l4_store_brs (int start, int count, l4_word_t *data)
{
  l4_word_t *br = __l4_utcb () + __L4_UTCB_BR0 - start;

  while (count--)
    *(data--) = *(br--);
}
