/* Copyright (C) 2003 Free Software Foundation, Inc.
   Written by Johan Rydberg.

   This file is part of the GNU Hurd.

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with this program; if not, write to the Free
   Software Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
   02111-1307 USA.  */

#if HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdlib.h>
#include <errno.h>
#include <sys/mman.h>
#include <assert.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <stdint.h>

#include "slab.h"


/* Number of pages the slab allocator has allocated.  */
static int __hurd_slab_nr_pages;

/* List of created slab spaces.  */
static hurd_slab_space_t space_list;

/* Protect __hurd_space_list.  */
static pthread_mutex_t list_lock = PTHREAD_MUTEX_INITIALIZER;

/* Buffer control structure.  Lives at the end of an object.  If the
   buffer is allocated, SLAB points to the slab to which it belongs.
   If the buffer is free, NEXT points to next buffer on free list.  */
union hurd_bufctl
{
  union hurd_bufctl *next;
  struct hurd_slab *slab;
};


/* When the allocator needs to grow a cache, it allocates a slab.  A
   slab consists of one or more pages of memory, split up into equally
   sized chunks.  */
struct hurd_slab
{
  struct hurd_slab *next;
  struct hurd_slab *prev;

  /* The reference counter holds the number of allocated chunks in
     the slab.  When the counter is zero, all chunks are free and
     the slab can be reqlinquished.  */
  int refcount;

  /* Single linked list of free buffers in the slab.  */
  union hurd_bufctl *free_list;
};


/* The type of a slab space.  */
struct hurd_slab_space
{
  /* Protects this structure, along with all the slabs.  */
  pthread_mutex_t lock;

  struct hurd_slab *slab_first;
  struct hurd_slab *slab_last;

  /* Link in the list of created slab spaces.  */
  struct hurd_slab_space *space_next;

  /* In the double linked list of slabs, empty slabs come first,
     after that there is the slabs that have some buffers allocated,
     and finally the complete slabs (refcount == 0).  FIRST_FREE
     points to the first non-empty slab.  */
  struct hurd_slab *first_free;

  /* For easy checking, this holds the value the reference counter
     should have for an empty slab.  */
  int full_refcount;

  /* The size of one object.  Should include possible alignment as
     well as the size of the bufctl structure.  */
  size_t size;

  /* The constructor.  */
  hurd_slab_constructor_t constructor;

  /* The destructor.  */
  hurd_slab_destructor_t destructor;

  /* The user's private data.  */
  void *hook;
};


/* Insert SLAB into the list of slabs in SPACE.  SLAB is expected to
   be complete (so it will be inserted at the end).  */
static void
insert_slab (struct hurd_slab_space *space, struct hurd_slab *slab)
{
  assert (slab->refcount == 0);
  if (space->slab_first == 0)
    space->slab_first = space->slab_last = slab;
  else
    {
      space->slab_last->next = slab;
      slab->prev = space->slab_last;
      space->slab_last = slab;
    }
}

/* Remove SLAB from list of slabs in SPACE.  */
static void
remove_slab (struct hurd_slab_space *space, struct hurd_slab *slab)
{
  if (slab != space->slab_first
      && slab != space->slab_last)
    {
      slab->next->prev = slab->prev;
      slab->prev->next = slab->next;
      return;
    }
  if (slab == space->slab_first)
    {
      space->slab_first = slab->next;
      if (space->slab_first)
	space->slab_first->prev = NULL;
    }
  if (slab == space->slab_last)
    {
      if (slab->prev)
	slab->prev->next = NULL;
      space->slab_last = slab->prev;
    }
}

/* Iterate through slabs in SPACE and release memory for slabs that
   are complete (no allocated buffers).  */
error_t
reap (struct hurd_slab_space *space)
{
  struct hurd_slab *s, *next, *new_first;
  error_t err = 0;

  pthread_mutex_lock (&space->lock);

  for (s = space->slab_first; s; s = next)
    {
      next = s->next;

      /* If the reference counter is zero there is no allocated
	 buffers, so it can be freed.  */
      if (!s->refcount)
	{
	  remove_slab (space, s);
	  
	  /* If there is a destructor it must be invoked for every 
	     buffer in the slab.  */
	  if (space->destructor)
	    {
	      union hurd_bufctl *bufctl;
	      for (bufctl = s->free_list; bufctl; bufctl = bufctl->next)
		{
		  void *buffer = (((void *) bufctl) 
				  - (space->size - sizeof *bufctl));
		  (*space->destructor) (space->hook, buffer);
		}
	    }
	  /* The slab is located at the end of the page (with the buffers
	     in front of it), get address by masking with page size.  
	     This frees the slab and all its buffers, since they live on
	     the same page.  */
	  err = munmap 
	    ((void *) (((uintptr_t) s) & ~(getpagesize () - 1)),
	     getpagesize ());
	  if (err)
	    break;
	  __hurd_slab_nr_pages--;
	}
    }

  /* Even in the case of an error, first_free must be updated since
     that slab may have been deallocated.  */
  new_first = space->slab_first;
  while (new_first)
    {
      if (new_first->refcount != space->full_refcount)
	break;
      new_first = new_first->next;
    }
  space->first_free = new_first;

  pthread_mutex_unlock (&space->lock);
  return err;
}

/* Release unused memory.  */
error_t
hurd_slab_reap (void)
{
  struct hurd_slab_space *space;
  error_t err = 0;

  pthread_mutex_lock (&list_lock);
  for (space = space_list; space; space = space->space_next)
    {
      if ((err = reap (space)))
	break;
    }
  pthread_mutex_unlock (&list_lock);
  return err;
}

/* SPACE has no more memory.  Allocate new slab and insert it into the
   list, repoint free_list and return possible error.  */
static error_t
grow (struct hurd_slab_space *space)
{
  struct hurd_slab *new_slab;
  union hurd_bufctl *bufctl;
  int nr_objs, i;
  void *p;

  p = mmap (NULL, getpagesize (), PROT_READ|PROT_WRITE,
	    MAP_PRIVATE|MAP_ANONYMOUS, 0, 0);
  if (p == MAP_FAILED)
    return errno;
  __hurd_slab_nr_pages++;

  new_slab = (p + getpagesize () - sizeof (struct hurd_slab));
  memset (new_slab, 0, sizeof (struct hurd_slab));

  /* Calculate the number of objects that the page can hold.
     SPACE->size should be adjusted to handle alignment.  */
  nr_objs = ((getpagesize () - sizeof (struct hurd_slab))
	     / space->size);
  
  for (i = 0; i < nr_objs; i++, p += space->size)
    {
      /* Invoke constructor at object creation time, not when it is
	 really allocated (for faster allocation).  */
      if (space->constructor)
	(*space->constructor) (space->hook, p);

      /* The most activity is in front of the object, so it is most
	 likely to be overwritten if a freed buffer gets accessed.
	 Therefor, put the bufctl structure at the end of the
	 object.  */
      bufctl = (p + space->size - sizeof *bufctl);
      bufctl->next = new_slab->free_list;
      new_slab->free_list = bufctl;
    }

  /* Insert slab into the list of available slabs for this cache.  The
     only time a slab should be allocated is when there is no more
     buffers, so it is safe to repoint first_free.  */  
  insert_slab (space, new_slab);
  space->first_free = new_slab;
  return 0;
}

/* Insert SPACE into list of created slab spaces.  */
static void
insert_space (struct hurd_slab_space *space)
{
  pthread_mutex_lock (&list_lock);
  if (!space_list)
    space_list = space;
  else
    {
      space->space_next = space_list;
      space_list = space;
    }
  pthread_mutex_unlock (&list_lock);
}


/* Create a new slab space with the given object size, alignment,
   constructor and destructor.  ALIGNMENT can be zero.  */
error_t
hurd_slab_create (size_t size, size_t alignment,
		  hurd_slab_constructor_t constructor,
		  hurd_slab_destructor_t destructor,
		  void *hook,
		  hurd_slab_space_t *space)
{
  error_t err;

  /* If SIZE is so big that one object can not fit into a page, return
     EINVAL.  FIXME: this doesn't take ALIGNMENT into account.  */
  if (size > (getpagesize () - sizeof (struct hurd_slab)
	      - sizeof (union hurd_bufctl)))
    return EINVAL;

  *space = malloc (sizeof (struct hurd_slab_space));
  if (!*space)
    return ENOMEM;
  memset (*space, 0, sizeof (struct hurd_slab_space));

  err = pthread_mutex_init (&(*space)->lock, NULL);
  if (err)
    {
      free (*space);
      return err;
    }

  /* The size should include the size of the buffer control
     structure.  */
  (*space)->size = size + sizeof (union hurd_bufctl);

  /* If the user has specified any alignment demand, simply round up
     the size of the buffer to the alignment.  */
  if (alignment)
    (*space)->size = (((*space)->size + alignment - 1) & ~(alignment - 1));

  (*space)->constructor = constructor;
  (*space)->destructor = destructor;
  (*space)->hook = hook;

  /* Calculate the number of objects that fit in a slab.  */
  (*space)->full_refcount 
    = ((getpagesize () - sizeof (struct hurd_slab)) / (*space)->size);

  insert_space (*space);
  return 0;
}


/* Allocate a new object from the slab space SPACE.  */
error_t
hurd_slab_alloc (hurd_slab_space_t space, void **buffer)
{
  error_t err;
  union hurd_bufctl *bufctl;

  pthread_mutex_lock (&space->lock);

  /* If there is no slabs with free buffer, the cache has to be
     expanded with another slab.  */
  if (!space->first_free)
    {
      err = grow (space);
      if (err)
	{
	  pthread_mutex_unlock (&space->lock);
	  return err;
	}
    }

  /* Remove buffer from the free list and update the reference
     counter.  If the reference counter will hit the top, it is
     handled at the time of the next allocation.  */
  bufctl = space->first_free->free_list;
  space->first_free->free_list = bufctl->next;
  space->first_free->refcount++;
  bufctl->slab = space->first_free;

  /* If the reference counter hits the top it means that there has
     been an allocation boost, otherwise dealloc would have updated
     the first_free pointer.  Find a slab with free objects.  */
  if (space->first_free->refcount == space->full_refcount)
    {
      struct hurd_slab *new_first = space->slab_first;
      while (new_first)
	{
	  if (new_first->refcount != space->full_refcount)
	    break;
	  new_first = new_first->next;
	}
      /* If first_free is set to NULL here it means that there are
	 only empty slabs.  The next call to alloc will allocate a new
	 slab if there was no call to dealloc in the meantime.  */
      space->first_free = new_first;
    }
  *buffer = ((void *) bufctl) - (space->size - sizeof *bufctl);
  pthread_mutex_unlock (&space->lock);
  return 0;
}


static inline void
put_on_slab_list (struct hurd_slab *slab, union hurd_bufctl *bufctl)
{
  bufctl->next = slab->free_list;
  slab->free_list = bufctl;
  slab->refcount--;
  assert (slab->refcount >= 0);
}


/* Deallocate the object BUFFER from the slab space SPACE.  */
void
hurd_slab_dealloc (hurd_slab_space_t space, void *buffer)
{
  struct hurd_slab *slab;
  union hurd_bufctl *bufctl;

  pthread_mutex_lock (&space->lock);

  bufctl = (buffer + (space->size - sizeof *bufctl));
  put_on_slab_list (slab = bufctl->slab, bufctl);

  /* Try to have first_free always pointing at the slab that has the
     most number of free objects.  So after this deallocation, update
     the first_free pointer if reference counter drops below the
     current reference counter of first_free.  */
  if (!space->first_free 
      || slab->refcount < space->first_free->refcount)
    space->first_free = slab;

  pthread_mutex_unlock (&space->lock);
}
