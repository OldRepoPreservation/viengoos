/* l4/compat/schedule.h - Public interface to the L4 scheduler.
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
# error "Never use <l4/compat/schedule.h> directly; include <l4/schedule.h> instead."
#endif


/* 3.1 Clock [Data Type]  */

/* Generic Programming Interface.  */

typedef struct
{
  L4_Word64_t raw;
} L4_Clock_t;


/* Convenience Programming Interface.  */

#if defined(__cplusplus)

#define _L4_CLOCK_OP(op, type)						\
static inline L4_Clock_t						\
_L4_attribute_always_inline						\
operator ## op ## (const L4_Clock_t clock, const type usec)		\
{									\
  L4_Clock_t new_clock;							\
  new_clock.raw = clock.raw op usec;					\
  return new_clock;							\
}

_L4_CLOCK_OP(+, int)
_L4_CLOCK_OP(+, L4_Word64_t)
_L4_CLOCK_OP(-, int)
_L4_CLOCK_OP(-, L4_Word64_t)
#undef _L4_CLOCK_OP


#define _L4_CLOCK_OP(op)						\
static inline L4_Bool_t							\
_L4_attribute_always_inline						\
operator ## op ## (const L4_Clock_t& clock1, const L4_Clock_t& clock2)	\
{									\
  return clock1.raw op clock2.raw;					\
}

_L4_CLOCK_OP(<)
_L4_CLOCK_OP(>)
_L4_CLOCK_OP(<=)
_L4_CLOCK_OP(>=)
_L4_CLOCK_OP(==)
_L4_CLOCK_OP(!=)
#undef _L4_CLOCK_OP

#else

#define _L4_CLOCK_OP(name, op)						\
static inline L4_Clock_t						\
_L4_attribute_always_inline						\
L4_Clock ## name ## Usec (const L4_Clock_t clock, const L4_Word64_t usec) \
{									\
  L4_Clock_t new_clock;							\
  new_clock.raw = clock.raw op usec;					\
  return new_clock;							\
}

_L4_CLOCK_OP(Add, +)
_L4_CLOCK_OP(Sub, -)
#undef _L4_CLOCK_OP


#define _L4_CLOCK_OP(name, op)						\
static inline L4_Bool_t							\
_L4_attribute_always_inline						\
L4_Clock ## name (const L4_Clock_t clock1, const L4_Clock_t clock2)	\
{									\
  return clock1.raw op clock2.raw;					\
}

_L4_CLOCK_OP(Earlier, <)
_L4_CLOCK_OP(Later, >)
_L4_CLOCK_OP(Equal, ==)
_L4_CLOCK_OP(NotEqual, !=)
#undef _L4_CLOCK_OP

#endif


/* 3.2 SystemClock [Systemcall]  */

/* Generic Programming Interface.  */

static inline L4_Clock_t
_L4_attribute_always_inline
L4_SystemClock (void)
{
  L4_Clock_t clock;

  clock.raw = _L4_system_clock ();
  return clock;
}


/* 3.3 Time [Data Type]  */

/* Generic Programming Interface.  */

typedef struct
{
  L4_Word16_t raw;
} L4_Time_t;


#define L4_Never	((L4_Time_t) { .raw = _L4_never })
#define L4_ZeroTime	((L4_Time_t) { .raw = _L4_zero_time })


static inline L4_Time_t
_L4_attribute_always_inline
L4_TimePeriod (L4_Word64_t microseconds)
{
  L4_Time_t tp;

  tp.raw = _L4_time_period (microseconds);
  return tp;
}


static inline L4_Time_t
_L4_attribute_always_inline
L4_TimePoint (L4_Clock_t at)
{
  L4_Time_t tp;

  tp.raw = _L4_time_point (at.raw);
  return tp;
}


/* Convenience Programming Interface.  */

#if defined(__cplusplus)

static inline L4_Time_t
_L4_attribute_always_inline
operator + (const L4_Time_t l, const L4_Word_t r)
{
  L4_Time_t new_time;
  new_time.raw = _L4_time_add_usec (l.raw, r);
  return new_time;
}


static inline L4_Time_t
_L4_attribute_always_inline
operator += (const L4_Time_t& l, const L4_Word_t r)
{
  l.raw = _L4_time_add_usec (l.raw, r);
  return l;
}


static inline L4_Time_t
_L4_attribute_always_inline
operator - (const L4_Time_t l, const L4_Word_t r)
{
  L4_Time_t new_time;
  new_time.raw = _L4_time_sub_usec (l.raw, r);
  return new_time;
}


static inline L4_Time_t
_L4_attribute_always_inline
operator -= (const L4_Time_t& l, const L4_Word_t r)
{
  l.raw = _L4_time_sub_usec (l.raw, r);
  return l;
}



static inline L4_Time_t
_L4_attribute_always_inline
operator + (L4_Time_t l, const L4_Time_t r)
{
  L4_Time_t new_time;
  new_time.raw = _L4_time_add (l.raw, r.raw);
  return new_time;
}


static inline L4_Time_t
_L4_attribute_always_inline
operator += (const L4_Time_t& l, const L4_Time_t r)
{
  l.raw = _L4_time_add (l.raw, r.raw);
  return l;
}


static inline L4_Time_t
_L4_attribute_always_inline
operator - (const L4_Time_t l, const L4_Time_t r)
{
  L4_Time_t new_time;
  new_time.raw = _L4_time_sub (l.raw, r.raw);
  return new_time;
}


static inline L4_Time_t
_L4_attribute_always_inline
operator -= (L4_Time_t& l, const L4_Time_t r)
{
  l.raw = _L4_time_sub (l.raw, r.raw);
  return l;
}


static inline L4_Time_t
_L4_attribute_always_inline
operator < (const L4_Time_t l, const L4_Time_t r)
{
  return _L4_is_time_shorter (l.raw, r.raw);
}


static inline L4_Time_t
_L4_attribute_always_inline
operator <= (const L4_Time_t l, const L4_Time_t r)
{
  return _L4_is_time_shorter (l.raw, r.raw)
    || _L4_is_time_equal (l.raw, r.raw);
}


static inline L4_Time_t
_L4_attribute_always_inline
operator > (const L4_Time_t l, const L4_Time_t r)
{
  return _L4_is_time_longer (l.raw, r.raw);
}


static inline L4_Time_t
_L4_attribute_always_inline
operator >= (const L4_Time_t l, const L4_Time_t r)
{
  return _L4_is_time_longer (l.raw, r.raw)
    || _L4_is_time_equal (l.raw, r.raw);
}


static inline L4_Time_t
_L4_attribute_always_inline
operator == (const L4_Time_t l, const L4_Time_t r)
{
  return _L4_is_time_equal (l.raw, r.raw);
}


static inline L4_Time_t
_L4_attribute_always_inline
operator != (const L4_Time_t l, const L4_Time_t r)
{
  return !_L4_is_time_not_equal (l.raw, r.raw);
}

#else

static inline L4_Time_t
_L4_attribute_always_inline
L4_TimeAddUsec (const L4_Time_t l, const L4_Word_t r)
{
  L4_Time_t new_time;
  new_time.raw = _L4_time_add_usec (l.raw, r);
  return new_time;
}


static inline L4_Time_t *
_L4_attribute_always_inline
L4_TimeAddUsecTo (L4_Time_t *l, const L4_Word_t r)
{
  l->raw = _L4_time_add_usec (l->raw, r);
  return l;
}


static inline L4_Time_t
_L4_attribute_always_inline
L4_TimeSubUsec (const L4_Time_t l, const L4_Word_t r)
{
  L4_Time_t new_time;
  new_time.raw = _L4_time_sub_usec (l.raw, r);
  return new_time;
}


static inline L4_Time_t *
_L4_attribute_always_inline
L4_TimeSubUsecFrom (L4_Time_t *l, const L4_Word_t r)
{
  l->raw = _L4_time_sub_usec (l->raw, r);
  return l;
}


static inline L4_Time_t
_L4_attribute_always_inline
L4_TimeAdd (const L4_Time_t l, const L4_Time_t r)
{
  L4_Time_t new_time;
  new_time.raw = _L4_time_add (l.raw, r.raw);
  return new_time;
}


static inline L4_Time_t *
_L4_attribute_always_inline
L4_TimeAddTo (L4_Time_t *l, const L4_Time_t r)
{
  l->raw = _L4_time_add (l->raw, r.raw);
  return l;
}


static inline L4_Time_t
_L4_attribute_always_inline
L4_TimeSub (const L4_Time_t l, const L4_Time_t r)
{
  L4_Time_t new_time;
  new_time.raw = _L4_time_sub (l.raw, r.raw);
  return new_time;
}


static inline L4_Time_t *
_L4_attribute_always_inline
L4_TimeSubFrom (L4_Time_t *l, const L4_Time_t r)
{
  l->raw = _L4_time_sub (l->raw, r.raw);
  return l;
}


static inline L4_Bool_t
_L4_attribute_always_inline
L4_IsTimeLonger (const L4_Time_t l, const L4_Time_t r)
{
  return _L4_is_time_longer (l.raw, r.raw);
}


static inline L4_Bool_t
_L4_attribute_always_inline
L4_IsTimeShorter (const L4_Time_t l, const L4_Time_t r)
{
  return _L4_is_time_shorter (l.raw, r.raw);
}


static inline L4_Bool_t
_L4_attribute_always_inline
L4_IsTimeEqual (const L4_Time_t l, const L4_Time_t r)
{
  return _L4_is_time_equal (l.raw, r.raw);
}


static inline L4_Bool_t
_L4_attribute_always_inline
L4_IsTimeNotEqual (const L4_Time_t l, const L4_Time_t r)
{
  return _L4_is_time_not_equal (l.raw, r.raw);
}

#endif	/* _cplusplus */


/* 3.4 ThreadSwitch [Systemcall]  */

/* Generic Programming Interface.  */

static inline void
_L4_attribute_always_inline
L4_ThreadSwitch (L4_ThreadId_t thread)
{
  _L4_thread_switch (thread.raw);
}


/* Convenience Programming Interface.  */

static inline void
_L4_attribute_always_inline
L4_Yield (void)
{
  _L4_thread_switch (_L4_nilthread);
}


/* 3.5 Schedule [Systemcall]  */

/* Generic Programming Interface.  */
static inline L4_Word_t
_L4_attribute_always_inline
L4_Schedule (L4_ThreadId_t dest, L4_Word_t time_control,
	     L4_Word_t proc_control, L4_Word_t prio,
	     L4_Word_t preempt_control, L4_Word_t *old_time_control)
{
  return _L4_schedule (dest.raw, time_control, proc_control, prio,
		       preempt_control, old_time_control);
}


/* Convenience Programming Interface.  */

static inline L4_Word_t
_L4_attribute_always_inline
L4_Set_Priority (L4_ThreadId_t dest, L4_Word_t prio)
{
  return _L4_set_priority (dest.raw, prio);
}


static inline L4_Word_t
_L4_attribute_always_inline
L4_Set_ProcessorNo (L4_ThreadId_t dest, L4_Word_t proc_no)
{
  return _L4_set_processor_no (dest.raw, proc_no);
}


static inline L4_Word_t
_L4_attribute_always_inline
L4_TimeSlice (L4_ThreadId_t dest, L4_Time_t *ts, L4_Time_t *tq)
{
  return _L4_time_slice (dest.raw, &ts->raw, &tq->raw);
}


static inline L4_Word_t
_L4_attribute_always_inline
L4_Set_TimeSlice (L4_ThreadId_t dest, L4_Time_t ts, L4_Time_t tq)
{
  return _L4_set_time_slice (dest.raw, ts.raw, tq.raw);
}


static inline L4_Word_t
_L4_attribute_always_inline
L4_Set_PreemptionDelay (L4_ThreadId_t dest, L4_Word_t sensitivePrio,
			L4_Word_t maxDelay)
{
  return _L4_set_preemption_delay (dest.raw, sensitivePrio, maxDelay);
}


static inline L4_Bool_t
_L4_attribute_always_inline
L4_EnablePreemptionFaultException (void)
{
  return _L4_enable_preemption_fault_exception ();
}


static inline L4_Bool_t
_L4_attribute_always_inline
L4_DisablePreemptionFaultException (void)
{
  return _L4_disable_preemption_fault_exception ();
}


static inline L4_Bool_t
_L4_attribute_always_inline
L4_EnablePreemption (void)
{
  return _L4_enable_preemption ();
}


static inline L4_Bool_t
_L4_attribute_always_inline
L4_DisablePreemption (void)
{
  return _L4_disable_preemption ();
}


static inline L4_Bool_t
_L4_attribute_always_inline
L4_PreemptionPending (void)
{
  return _L4_preemption_pending ();
}
