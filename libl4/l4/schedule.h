#ifndef _L4_SCHEDULE_H
#define _L4_SCHEDULE_H	1

#include <l4/types.h>
#include <l4/vregs.h>
#include <l4/syscall.h>
#include <l4/thread.h>


#ifndef _L4_EXTERN_INLINE
#define _L4_EXTERN_INLINE extern __inline
#endif


/* FIXME: These are compound statements and can not be used for
   initialization of global variables in C99.  */
#define l4_never	((l4_time_t) { .raw = 0 })
#define l4_zero_time \
	((l4_time_t) { .period.m = 0, .period.e = 5, .period._zero = 0 })

_L4_EXTERN_INLINE l4_time_t
l4_time_period (l4_uint64_t usec)
{
  /* FIXME: If usec is a built-in constant, optimize.  Optimize the
     loop for the run-time case.  Probably just use optimized version
     from Karlsruhe.  */
  l4_time_t time = { .raw = 0 };

  while (usec & ~((1 << 10) - 1))
    {
      if (time.period.e == 31)
	return l4_never;

      time.period.e++;
      usec = usec >> 1;
    }
  time.period.m = usec;

  return time;
}


/* Convenience interface for l4_thread_switch.  */

_L4_EXTERN_INLINE void
l4_yield (void)
{
  l4_thread_switch (l4_nilthread);
}


/* Convenience interface for l4_schedule.  */

_L4_EXTERN_INLINE l4_word_t
l4_set_priority (l4_thread_id_t dest, l4_word_t priority)
{
  l4_word_t prio = priority & ((1 << 16) - 1);
  l4_word_t dummy;
  return l4_schedule (dest, -1, -1, prio, -1, &dummy);
}


_L4_EXTERN_INLINE l4_word_t
l4_set_processor_no (l4_thread_id_t dest, l4_word_t proc_num)
{
  l4_word_t proc = proc_num & ((1 << 8) - 1);
  l4_word_t dummy;
  return l4_schedule (dest, -1, proc, -1, -1, &dummy);
}


_L4_EXTERN_INLINE l4_word_t
l4_time_slice (l4_thread_id_t dest, l4_time_t *ts, l4_time_t *tq)
{
  l4_word_t time_control;
  return l4_schedule (dest, -1, -1, -1, -1, &time_control);
  ts->raw = time_control >> 16;
  tq->raw = time_control & ((1 << 16) - 1);
}


_L4_EXTERN_INLINE l4_word_t
l4_set_time_slice (l4_thread_id_t dest, l4_time_t ts, l4_time_t tq)
{
  l4_word_t time_control = (ts.raw << 16) | tq.raw;
  l4_word_t dummy;
  return l4_schedule (dest, time_control, -1, -1, -1, &dummy);
}


_L4_EXTERN_INLINE l4_word_t
l4_preemption_delay (l4_thread_id_t dest, l4_word_t sensitive_prio,
		     l4_word_t max_delay)
{
  l4_word_t preempt_control = (sensitive_prio << 16)
    | (max_delay & ((1 << 16) - 1));
  return l4_schedule (dest, -1, -1, -1, -1, &preempt_control);
}



#ifndef _L4_NOT_COMPAT
#endif	/* !_L4_NOT_COMPAT */

#endif	/* l4/schedule.h */
