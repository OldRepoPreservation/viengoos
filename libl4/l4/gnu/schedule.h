/* l4/gnu/schedule.h - Public GNU interface for L4 scheduling interface.
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

#ifndef _L4_SCHEDULE_H
# error "Never use <l4/gnu/schedule.h> directly; include <l4/schedule.h> instead."
#endif


typedef _L4_clock_t l4_clock_t;


static inline l4_clock_t
_L4_attribute_always_inline
l4_SystemClock (void)
{
  return _L4_system_clock ();
}


typedef _L4_time_t l4_time_t;

#define L4_NEVER	_L4_never
#define L4_ZERO_TIME	_L4_zero_time

#define L4_TIME_PERIOD_MAX	_L4_TIME_PERIOD_MAX

static inline l4_time_t
_L4_attribute_always_inline
l4_time_period (l4_uint64_t usec)
{
  return _L4_time_period (usec);
}


static inline l4_time_t
_L4_attribute_always_inline
l4_time_mul2 (l4_time_t t)
{
  __L4_time_t _t;

  _t.raw = t;
  if (_t.period.e == _L4_TIME_PERIOD_E_MAX)
    return L4_NEVER;
  _t.period.e++;
  return _t.raw;
}


static inline l4_time_t
_L4_attribute_always_inline
l4_time_point (l4_uint64_t at)
{
  return _L4_time_point (at);
}


static inline l4_time_t
_L4_attribute_always_inline
l4_time_add_usec (l4_time_t l, l4_word_t r)
{
  return _L4_time_add_usec (l, r);
}


static inline l4_time_t
_L4_attribute_always_inline
l4_time_sub_usec (l4_time_t l, l4_word_t r)
{
  return _L4_time_sub_usec (l, r);
}


static inline l4_time_t
_L4_attribute_always_inline
l4_time_add (l4_time_t l, l4_time_t r)
{
  return _L4_time_add (l, r);
}


static inline l4_time_t
_L4_attribute_always_inline
l4_time_sub (l4_time_t l, l4_time_t r)
{
  return _L4_time_sub (l, r);
}


static inline l4_time_t
_L4_attribute_always_inline
l4_is_time_longer (l4_time_t l, l4_time_t r)
{
  return _L4_time_longer (l, r);
}


static inline l4_time_t
_L4_attribute_always_inline
l4_is_time_shorter (l4_time_t l, l4_time_t r)
{
  return _L4_time_shorter (l, r);
}


static inline l4_time_t
_L4_attribute_always_inline
l4_is_time_equal (l4_time_t l, l4_time_t r)
{
  return _L4_time_equal (l, r);
}


static inline l4_time_t
_L4_attribute_always_inline
l4_is_time_not_equal (l4_time_t l, l4_time_t r)
{
  return _L4_time_not_equal (l, r);
}



static inline void
_L4_attribute_always_inline
l4_thread_switch (l4_thread_id_t thread)
{
  _L4_thread_switch (thread);
}


static inline void
_L4_attribute_always_inline
l4_yield (void)
{
  _L4_yield ();
}



/* Generic Programming Interface.  */
static inline l4_word_t
_L4_attribute_always_inline
l4_schedule (l4_thread_id_t dest, l4_word_t time_control,
	     l4_word_t proc_control, l4_word_t prio,
	     l4_word_t preempt_control, l4_word_t *old_time_control)
{
  return _L4_schedule (dest, time_control, proc_control, prio,
		       preempt_control, old_time_control);
}


static inline l4_word_t
_L4_attribute_always_inline
l4_set_priority (l4_thread_id_t dest, l4_word_t priority)
{
  return _L4_set_priority (dest, priority);
}


static inline l4_word_t
_L4_attribute_always_inline
l4_set_processor_no (l4_thread_id_t dest, l4_word_t proc_num)
{
  return _L4_set_processor_no (dest, proc_num);
}


static inline l4_word_t
_L4_attribute_always_inline
l4_time_slice (l4_thread_id_t dest, l4_time_t *ts,
				       l4_time_t *tq)
{
  return _L4_time_slice (dest, ts, tq);
}


static inline l4_word_t
_L4_attribute_always_inline
l4_set_time_slice (l4_thread_id_t dest, l4_time_t ts, l4_time_t tq)
{
  return _L4_set_time_slice (dest, ts, tq);
}


static inline l4_word_t
_L4_attribute_always_inline
l4_set_preemption_delay (l4_thread_id_t dest, l4_word_t sensitive_prio,
			 l4_word_t max_delay)
{
  return _L4_set_preemption_delay (dest, sensitive_prio, max_delay);
}


static inline l4_word_t
_L4_attribute_always_inline
l4_enable_preemption_fault_exception (void)
{
  return _L4_enable_preemption_fault_exception ();
}


static inline l4_word_t
_L4_attribute_always_inline
l4_disable_preemption_fault_exception (void)
{
  return _L4_disable_preemption_fault_exception ();
}


static inline l4_word_t
_L4_attribute_always_inline
l4_enable_preemption (void)
{
  return _L4_enable_preemption ();
}


static inline l4_word_t
_L4_attribute_always_inline
l4_disable_preemption (void)
{
  return _L4_disable_preemption ();
}


static inline l4_word_t
_L4_attribute_always_inline
l4_preemptionpending (void)
{
  return _L4_preemption_pending ();
}
