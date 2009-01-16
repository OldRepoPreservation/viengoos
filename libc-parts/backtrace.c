/* backtrace.c - Gather a backtrace.
   Copyright (C) 2008 Free Software Foundation, Inc.
   Written by Neal H. Walfield <neal@gnu.org>.

   The GNU Hurd is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 3 of the
   License, or (at your option) any later version.

   The GNU Hurd is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see
   <http://www.gnu.org/licenses/>.  */

#include <s-printf.h>

#ifndef RM_INTERN
# include <hurd/thread.h>
#endif
#ifdef USE_L4
# include <l4.h>
#endif

#ifdef RM_INTERN
# define RA(level)							\
  if (level < size && __builtin_frame_address ((level) + 1))		\
    {									\
      array[level] = __builtin_return_address ((level) + 1);		\
      if (array[level] == 0)						\
	return count;							\
      count ++;								\
    }									\
  else									\
    return count;

#else
# include <hurd/thread.h>
# include <setjmp.h>

# define RA(level)						\
  if (count >= size)						\
    return count;						\
								\
  {								\
    void *fa = __builtin_frame_address ((level) + 1);		\
    if (fa)							\
      {								\
	if (utcb)						\
	  {							\
	    catcher.start = (uintptr_t) fa + displacement;	\
	    catcher.len = sizeof (uintptr_t);			\
	    catcher.callback = get_me_outda_here;		\
								\
	    hurd_fault_catcher_register (&catcher);		\
	  }							\
								\
	array[count] = __builtin_return_address ((level) + 1);	\
								\
	if (utcb)						\
	  hurd_fault_catcher_unregister (&catcher);		\
								\
	if (array[count] == 0)					\
	  return count;						\
	count ++;						\
      }								\
    else							\
      return count;						\
  }
#endif


int
backtrace (void **array, int size)
{
  /* Without the volatile, count ends up either optimized away or in a
     caller saved register before the setjmp.  In either case, if we
     fault, we'll end up returning 0 even if we get some of the
     backtrace.  volatile seems to prevent this.  */
  volatile int count = 0;

#ifndef RM_INTERN
  /* The location of the return address relative to the start of a
     frame.  */
  intptr_t displacement = sizeof (uintptr_t);
# ifndef i386
#  warning Not ported to this architecture... guessing
# endif

  jmp_buf jmpbuf;

  /* If we don't yet have a utcb then don't set up a fault catcher.  */
  struct vg_utcb *utcb = hurd_utcb ();

  if (utcb)
    {
      if (setjmp (jmpbuf))
	return count;
    }

  struct hurd_fault_catcher catcher;

  bool get_me_outda_here (struct activation_frame *af, uintptr_t fault)
  {
    hurd_fault_catcher_unregister (&catcher);
    hurd_activation_frame_longjmp (af, jmpbuf, true, 1);
    return true;
  }
#endif

  RA(0);
  RA(1);
  RA(2);
  RA(3);
  RA(4);
  RA(5);
  RA(6);
  RA(7);
  RA(9);
  RA(10);
  RA(11);
  RA(12);
  RA(13);
  RA(14);
  RA(15);
  RA(16);
  RA(17);
  RA(18);
  RA(19);
  RA(20);
  return count;
}

void
backtrace_print (void)
{
  void *bt[20];
  int count = backtrace (bt, sizeof (bt) / sizeof (bt[0]));

#ifdef USE_L4
  s_printf ("Backtrace for %x: ", l4_myself ());
#else
# ifndef RM_INTERN
  s_printf ("Backtrace for %x: ", hurd_myself ());
# else
#  warning Don't know how to get tid.
  s_printf ("Backtrace: ");
# endif
#endif

  int i;
  for (i = 0; i < count; i ++)
    s_printf ("%p ", bt[i]);
  s_printf ("\n");
}
