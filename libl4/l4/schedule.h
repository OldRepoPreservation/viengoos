/* l4/schedule.h - Public interface to the L4 scheduler.
   Copyright (C) 2003, 2004 Free Software Foundation, Inc.
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
#define _L4_SCHEDULE_H	1

#include <l4/types.h>
#include <l4/vregs.h>
#include <l4/syscall.h>
#include <l4/thread.h>


typedef _L4_RAW
(_L4_time_t, _L4_STRUCT2
 ({
   /* This is a time period.  It is 2^e * m usec long.  */
   _L4_BITFIELD3
     (_L4_uint16_t,
      _L4_BITFIELD (m, 10),
      _L4_BITFIELD (e, 5),
      _L4_BITFIELD (_zero, 1));
 } period,
 {
   /* This is a time point with validity (2^10 - 1) * 2^e.  */
   _L4_BITFIELD4
     (_L4_uint16_t,
      _L4_BITFIELD (m, 10),
      _L4_BITFIELD (c, 1),
      _L4_BITFIELD (e, 4),
      _L4_BITFIELD (_one, 1));
 } point)) __L4_time_t;


#define _L4_never	((_L4_time_t) 0)
/* _L4_zero_time is a time period with mantisse 0 and exponent 1.  */
#define _L4_zero_time	((_L4_time_t) (1 << 10))


static inline _L4_time_t
_L4_attribute_always_inline
_L4_time_add_usec (_L4_time_t time, _L4_word_t usec)
{
  /* FIXME: Implement me.  */
  return 0;
}


static inline _L4_time_t
_L4_attribute_always_inline
_L4_time_sub_usec (_L4_time_t time, _L4_word_t usec)
{
  /* FIXME: Implement me.  */
  return 0;
}


static inline _L4_time_t
_L4_attribute_always_inline
_L4_time_add (_L4_time_t time1, _L4_time_t time2)
{
  /* FIXME: Implement me.  */
  return 0;
}


static inline _L4_time_t
_L4_attribute_always_inline
_L4_time_sub (_L4_time_t time1, _L4_time_t time2)
{
  /* FIXME: Implement me.  */
  return 0;
}


static inline _L4_word_t
_L4_attribute_always_inline
_L4_is_time_longer (_L4_time_t time1, _L4_time_t time2)
{
  /* FIXME: Implement me.  */
  return 0;
}


static inline _L4_word_t
_L4_attribute_always_inline
_L4_is_time_shorter (_L4_time_t time1, _L4_time_t time2)
{
  /* FIXME: Implement me.  */
  return 0;
}


static inline _L4_word_t
_L4_attribute_always_inline
_L4_is_time_equal (_L4_time_t time1, _L4_time_t time2)
{
  /* FIXME: Implement me.  */
  return 0;
}


static inline _L4_word_t
_L4_attribute_always_inline
_L4_is_time_not_equal (_L4_time_t time1, _L4_time_t time2)
{
  /* FIXME: Implement me.  */
  return 0;
}



static inline _L4_time_t
_L4_attribute_always_inline
_L4_time_period (_L4_uint64_t usec)
{
  /* FIXME: If usec is a built-in constant, optimize.  Optimize the
     loop for the run-time case.  Probably just use optimized version
     from Karlsruhe.  */
  __L4_time_t time = { .raw = 0 };

  while (usec & ~((1 << 10) - 1))
    {
      if (time.period.e == 31)
	return _L4_never;

      time.period.e++;
      usec = usec >> 1;
    }
  time.period.m = usec;

  return time.raw;
}


static inline _L4_time_t
_L4_attribute_always_inline
_L4_time_point (_L4_uint64_t at)
{
  /* FIXME: Implement me.  */
  return 0;
}


/* Convenience interface for l4_thread_switch.  */

static inline void
_L4_attribute_always_inline
_L4_yield (void)
{
  _L4_thread_switch (_L4_nilthread);
}


/* Convenience interface for _L4_schedule.  */

static inline _L4_word_t
_L4_attribute_always_inline
_L4_set_priority (_L4_thread_id_t dest, _L4_word_t priority)
{
  _L4_word_t prio = priority & ((1 << 16) - 1);
  _L4_word_t dummy;
  return _L4_schedule (dest, -1, -1, prio, -1, &dummy);
}


static inline _L4_word_t
_L4_attribute_always_inline
_L4_set_processor_no (_L4_thread_id_t dest, _L4_word_t proc_num)
{
  _L4_word_t proc = proc_num & ((1 << 8) - 1);
  _L4_word_t dummy;
  return _L4_schedule (dest, -1, proc, -1, -1, &dummy);
}


static inline _L4_word_t
_L4_attribute_always_inline
_L4_time_slice (_L4_thread_id_t dest, _L4_time_t *ts, _L4_time_t *tq)
{
  _L4_word_t time_control;
  return _L4_schedule (dest, -1, -1, -1, -1, &time_control);
  *ts = time_control >> 16;
  *tq = time_control & ((1 << 16) - 1);
}


static inline _L4_word_t
_L4_attribute_always_inline
_L4_set_time_slice (_L4_thread_id_t dest, _L4_time_t ts, _L4_time_t tq)
{
  _L4_word_t time_control = (ts << 16) | tq;
  _L4_word_t dummy;
  return _L4_schedule (dest, time_control, -1, -1, -1, &dummy);
}


static inline _L4_word_t
_L4_attribute_always_inline
_L4_set_preemption_delay (_L4_thread_id_t dest, _L4_word_t sensitive_prio,
			  _L4_word_t max_delay)
{
  _L4_word_t preempt_control = (sensitive_prio << 16)
    | (max_delay & ((1 << 16) - 1));
  return _L4_schedule (dest, -1, -1, -1, -1, &preempt_control);
}


/* Now incorporate the public interfaces the user has selected.  */
#ifdef _L4_INTERFACE_L4
#include <l4/compat/schedule.h>
#endif
#ifdef _L4_INTERFACE_GNU
#include <l4/gnu/schedule.h>
#endif

#endif	/* l4/schedule.h */
