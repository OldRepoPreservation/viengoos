/* l4/bits/vregs.h - L4 virtual registers for powerpc.
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
static inline _L4_word_t *
_L4_attribute_always_inline _L4_attribute_const
_L4_utcb (void)
{
  _L4_word_t *utcb;

  __asm__ ("mr %[utcb], %%r2"
	   : [utcb] "=r" (utcb));

  return utcb;
}

/* Offsets of various elements in the UTCB, relativ to the UTCB
   address.  */
#define _L4_UTCB_MR0		0
#define _L4_UTCB_THREAD_WORD0	-4
#define _L4_UTCB_THREAD_WORD1	-5
#define _L4_UTCB_SENDER		-6
#define _L4_UTCB_RECEIVER	-7
#define _L4_UTCB_TIMEOUT	-8
#define _L4_UTCB_ERROR_CODE	-9
#define _L4_UTCB_FLAGS		-10
#define _L4_UTCB_EXC_HANDLER	-11
#define _L4_UTCB_PAGER		-12
#define _L4_UTCB_USER_HANDLE	-13
#define _L4_UTCB_PROCESSOR_NO	-14
#define _L4_UTCB_MY_GLOBAL_ID	-15
#define _L4_UTCB_BR0		-16


/* Get the local thread ID.  */
static inline _L4_thread_id_t
_L4_attribute_always_inline
_L4_my_local_id (void)
{
  /* Local thread ID is equal to the UTCB address.  */
  return (_L4_word_t) _L4_utcb ();
}


/* Get the global thread ID.  */
static inline _L4_thread_id_t
_L4_attribute_always_inline
_L4_my_global_id (void)
{
  _L4_word_t *utcb = _L4_utcb ();

  return utcb[_L4_UTCB_MY_GLOBAL_ID];
}


static inline int
_L4_attribute_always_inline
_L4_processor_no (void)
{
  _L4_word_t *utcb = _L4_utcb ();

  return utcb[_L4_UTCB_PROCESSOR_NO];
}


static inline _L4_word_t
_L4_attribute_always_inline
_L4_user_defined_handle (void)
{
  _L4_word_t *utcb = _L4_utcb ();

  return utcb[_L4_UTCB_USER_HANDLE];
}


static inline void
_L4_attribute_always_inline
_L4_set_user_defined_handle (_L4_word_t data)
{
  _L4_word_t *utcb = _L4_utcb ();

  utcb[_L4_UTCB_USER_HANDLE] = data;
}


static inline _L4_thread_id_t
_L4_attribute_always_inline
_L4_pager (void)
{
  _L4_word_t *utcb = _L4_utcb ();

  return utcb[_L4_UTCB_PAGER];
}


static inline void
_L4_attribute_always_inline
_L4_set_pager (_L4_thread_id_t thread)
{
  _L4_word_t *utcb = _L4_utcb ();

  utcb[_L4_UTCB_PAGER] = thread;
}


static inline _L4_thread_id_t
_L4_attribute_always_inline
_L4_exception_handler (void)
{
  _L4_word_t *utcb = _L4_utcb ();

  return utcb[_L4_UTCB_EXC_HANDLER];
}


static inline void
_L4_attribute_always_inline
_L4_set_exception_handler (_L4_thread_id_t thread)
{
  _L4_word_t *utcb = _L4_utcb ();

  utcb[_L4_UTCB_EXC_HANDLER] = thread;
}


static inline void
_L4_attribute_always_inline
_L4_clr_cop_flag (_L4_word_t n)
{
  _L4_word_t *utcb = _L4_utcb ();

  /* The second byte in the flags field contains the coprocessor
     flags.  */
  utcb[_L4_UTCB_FLAGS] &= ~(1 << (8 + n));
}


static inline void
_L4_attribute_always_inline
_L4_set_cop_flag (_L4_word_t n)
{
  _L4_word_t *utcb = _L4_utcb ();

  /* The second byte in the flags field contains the coprocessor
     flags.  */
  utcb[_L4_UTCB_FLAGS] |= 1 << (8 + n);
}


static inline _L4_word_t
_L4_attribute_always_inline
_L4_disable_preemption_fault_exception (void)
{
  _L4_word_t *utcb = _L4_utcb ();
  _L4_word_t result;

  /* The first byte in the flags field contains the preemption
     flags.  The sixth bit is the signal bit.  */
  result = (utcb[_L4_UTCB_FLAGS] >> 5) & 1;
  utcb[_L4_UTCB_FLAGS] &= ~(1 << 5);

  return result;
}


static inline _L4_word_t
_L4_attribute_always_inline
_L4_enable_preemption_fault_exception (void)
{
  _L4_word_t *utcb = _L4_utcb ();
  _L4_word_t result;

  /* The first byte in the flags field contains the preemption
     flags.  The sixth bit is the signal flag.  */
  result = (utcb[_L4_UTCB_FLAGS] >> 5) & 1;
  utcb[_L4_UTCB_FLAGS] |= 1 << 5;

  return result;
}


static inline _L4_word_t
_L4_attribute_always_inline
_L4_disable_preemption (void)
{
  _L4_word_t *utcb = _L4_utcb ();
  _L4_word_t result;

  /* The first byte in the flags field contains the preemption
     flags.  The seventh bit is the delay flag.  */
  result = (utcb[_L4_UTCB_FLAGS] >> 6) & 1;
  utcb[_L4_UTCB_FLAGS] &= ~(1 << 6);

  return result;
}


static inline _L4_word_t
_L4_attribute_always_inline
_L4_enable_preemption (void)
{
  _L4_word_t *utcb = _L4_utcb ();
  _L4_word_t result;

  /* The first byte in the flags field contains the preemption
     flags.  The seventh bit is the delay flag.  */
  result = (utcb[_L4_UTCB_FLAGS] >> 6) & 1;
  utcb[_L4_UTCB_FLAGS] |= 1 << 6;

  return result;
}


static inline _L4_word_t
_L4_attribute_always_inline
_L4_preemption_pending (void)
{
  _L4_word_t *utcb = _L4_utcb ();
  _L4_word_t result;

  result = (utcb[_L4_UTCB_FLAGS] >> 7) & 1;
  utcb[_L4_UTCB_FLAGS] &= ~(1 << 7);

  return result;
}


static inline _L4_word_t
_L4_attribute_always_inline
_L4_error_code (void)
{
  _L4_word_t *utcb = _L4_utcb ();

  return utcb[_L4_UTCB_ERROR_CODE];
}


static inline _L4_word_t
_L4_attribute_always_inline
_L4_xfer_timeouts (void)
{
  _L4_word_t *utcb = _L4_utcb ();

  return utcb[_L4_UTCB_TIMEOUT];
}


static inline void
_L4_attribute_always_inline
_L4_set_xfer_timeouts (_L4_word_t time)
{
  _L4_word_t *utcb = _L4_utcb ();

  utcb[_L4_UTCB_TIMEOUT] = time;
}


static inline _L4_thread_id_t
_L4_attribute_always_inline
_L4_intended_receiver (void)
{
  _L4_word_t *utcb = _L4_utcb ();

  return utcb[_L4_UTCB_RECEIVER];
}


static inline _L4_thread_id_t
_L4_attribute_always_inline
_L4_actual_sender (void)
{
  _L4_word_t *utcb = _L4_utcb ();

  return utcb[_L4_UTCB_SENDER];
}


static inline void
_L4_attribute_always_inline
_L4_set_virtual_sender (_L4_thread_id_t thread)
{
  _L4_word_t *utcb = _L4_utcb ();

  utcb[_L4_UTCB_SENDER] = thread;
}


/* Message registers (MR0 to MR63) start at offset _L4_UTCB_MR0 and
   go upward.  */

/* Set message register NR to DATA.  */
static inline void
_L4_attribute_always_inline
_L4_load_mr (int nr, _L4_word_t data)
{
  _L4_word_t *mr = _L4_utcb () + _L4_UTCB_MR0;

  mr[nr] = data;
}


/* Set COUNT message registers beginning from START to the values in
   DATA.  */
static inline void
_L4_attribute_always_inline
_L4_load_mrs (int start, int count, _L4_word_t *data)
{
  _L4_word_t *mr = _L4_utcb () + _L4_UTCB_MR0 + start;

  while (count--)
    *(mr++) = *(data++);
}


/* Store message register NR in DATA.  */
static inline void
_L4_attribute_always_inline
_L4_store_mr (int nr, _L4_word_t *data)
{
  _L4_word_t *mr = _L4_utcb () + _L4_UTCB_MR0;

  *data = mr[nr];
}


/* Store COUNT message registers beginning from START in DATA.  */
static inline void
_L4_attribute_always_inline
_L4_store_mrs (int start, int count, _L4_word_t *data)
{
  _L4_word_t *mr = _L4_utcb () + _L4_UTCB_MR0 + start;

  while (count--)
    *(data++) = *(mr++);
}


/* Buffer registers (BR0 to BR31) start at offset _L4_UTCB_BR0 and go
   downward.  */

/* Set message register NR to DATA.  */
static inline void
_L4_attribute_always_inline
_L4_load_br (int nr, _L4_word_t data)
{
  _L4_word_t *br = _L4_utcb () + _L4_UTCB_BR0;

  br[-nr] = data;
}


/* Set COUNT message registers beginning from START to the values in
   DATA.  */
static inline void
_L4_attribute_always_inline
_L4_load_brs (int start, int count, _L4_word_t *data)
{
  _L4_word_t *br = _L4_utcb () + _L4_UTCB_BR0 - start;

  while (count--)
    *(br--) = *(data++);
}


/* Store message register NR in DATA.  */
static inline void
_L4_attribute_always_inline
_L4_store_br (int nr, _L4_word_t *data)
{
  _L4_word_t *br = _L4_utcb () + _L4_UTCB_BR0;

  *data = br[-nr];
}


/* Store COUNT message registers beginning from START in DATA.  */
static inline void
_L4_attribute_always_inline
_L4_store_brs (int start, int count, _L4_word_t *data)
{
  _L4_word_t *br = _L4_utcb () + _L4_UTCB_BR0 - start;

  while (count--)
    *(data++) = *(br--);
}
