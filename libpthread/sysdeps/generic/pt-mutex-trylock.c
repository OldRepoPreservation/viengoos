/* Try to Lock a mutex.  Generic version.
   Copyright (C) 2002, 2005, 2008 Free Software Foundation, Inc.
   This file is part of the GNU C Library.

   The GNU C Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   The GNU C Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public
   License along with the GNU C Library; see the file COPYING.LIB.  If not,
   write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.  */

#include <pthread.h>

#include <pt-internal.h>

#define LOSE do { * (int *) 0 = 0; } while (1)

/* Lock MUTEX, return EBUSY if we can't get it.  */
int
__pthread_mutex_trylock (struct __pthread_mutex *mutex)
{
  int err;
  struct __pthread *self;

  __pthread_spin_lock (&mutex->__lock);
  if (__pthread_spin_trylock (&mutex->__held) == 0)
    /* Acquired the lock.  */
    {
#ifndef NDEBUG
      self = _pthread_self ();
      if (self)
	/* The main thread may take a lock before the library is fully
	   initialized, in particular, before the main thread has a
	   TCB.  */
	{
	  assert (! mutex->owner);
	  mutex->owner = _pthread_self ();
	}
#endif

      if (mutex->attr)
	switch (mutex->attr->mutex_type)
	  {
	  case PTHREAD_MUTEX_NORMAL:
	    break;

	  case PTHREAD_MUTEX_RECURSIVE:
	    mutex->locks = 1;
	  case PTHREAD_MUTEX_ERRORCHECK:
	    mutex->owner = _pthread_self ();
	    break;

	  default:
	    LOSE;
	  }

      __pthread_spin_unlock (&mutex->__lock);
      return 0;
    }

  err = EBUSY;

  if (mutex->attr)
    {
      self = _pthread_self ();
      switch (mutex->attr->mutex_type)
	{
	case PTHREAD_MUTEX_NORMAL:
	  break;

	case PTHREAD_MUTEX_ERRORCHECK:
	  /* We could check if MUTEX->OWNER is SELF, however, POSIX
	     does not permit pthread_mutex_trylock to return EDEADLK
	     instead of EBUSY, only pthread_mutex_lock.  */
	  break;

	case PTHREAD_MUTEX_RECURSIVE:
	  if (mutex->owner == self)
	    {
	      mutex->locks ++;
	      err = 0;
	    }
	  break;

	default:
	  LOSE;
	}
    }

  __pthread_spin_unlock (&mutex->__lock);

  return err;
}

strong_alias (__pthread_mutex_trylock, _pthread_mutex_trylock);
strong_alias (__pthread_mutex_trylock, pthread_mutex_trylock);
