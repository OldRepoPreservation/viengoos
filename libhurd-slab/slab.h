/* slab.h - The GNU Hurd slab allocator interface.
   Copyright (C) 2003, 2005 Free Software Foundation, Inc.
   Written by Marcus Brinkmann <marcus@gnu.org>

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

#ifndef _HURD_SLAB_H
#define _HURD_SLAB_H	1

#include <errno.h>
#include <stdbool.h>
#include <pthread.h>


/* Allocate a buffer in *PTR of size SIZE which must be a power of 2
   and self aligned (i.e. aligned on a SIZE byte boundary).  HOOK is
   as provided to hurd_slab_create.  Return 0 on success, an error
   code on failure.  */
typedef error_t (*hurd_slab_allocate_buffer_t) (void *hook, size_t size,
						void **ptr);

/* Deallocate buffer BUFFER of size SIZE.  HOOK is as provided to
   hurd_slab_create.  */
typedef error_t (*hurd_slab_deallocate_buffer_t) (void *hook, void *buffer,
						  size_t size);

/* Initialize the slab object pointed to by BUFFER.  HOOK is as
   provided to hurd_slab_create.  */
typedef error_t (*hurd_slab_constructor_t) (void *hook, void *buffer);

/* Destroy the slab object pointed to by BUFFER.  HOOK is as provided
   to hurd_slab_create.  */
typedef void (*hurd_slab_destructor_t) (void *hook, void *buffer);


/* The type of a slab space.  

   The structure is divided into two parts: the first is only used
   while the slab space is constructed.  Its fields are either
   initialized by a static initializer (HURD_SLAB_SPACE_INITIALIZER)
   or by the hurd_slab_create function.  The initialization of the
   space is delayed until the first allocation.  After that only the
   second part is used.  */

typedef struct hurd_slab_space *hurd_slab_space_t;
struct hurd_slab_space
{
  /* First part.  Used when initializing slab space object.   */
  
  /* True if slab space has been initialized.  */
  bool initialized;
  
  /* Protects this structure, along with all the slabs.  No need to
     delay initialization of this field.  */
  pthread_mutex_t lock;

  /* The size and alignment of objects allocated using this slab
     space.  These to fields are used to calculate the final object
     size, which is put in SIZE (defined below).  */
  size_t requested_size;
  size_t requested_align;

  /* The buffer allocator.  */
  hurd_slab_allocate_buffer_t allocate_buffer;

  /* The buffer deallocator.  */
  hurd_slab_deallocate_buffer_t deallocate_buffer;

  /* The constructor.  */
  hurd_slab_constructor_t constructor;

  /* The destructor.  */
  hurd_slab_destructor_t destructor;

  /* The user's private data.  */
  void *hook;

  /* Second part.  Runtime information for the slab space.  */

  struct hurd_slab *slab_first;
  struct hurd_slab *slab_last;

  /* In the doubly-linked list of slabs, empty slabs come first, after
     that the slabs that have some buffers allocated, and finally the
     complete slabs (refcount == 0).  FIRST_FREE points to the first
     non-empty slab.  */
  struct hurd_slab *first_free;

  /* For easy checking, this holds the value the reference counter
     should have for an empty slab.  */
  int full_refcount;

  /* The size of one object.  Should include possible alignment as
     well as the size of the bufctl structure.  */
  size_t size;
};


/* Static initializer.  TYPE is used to get size and alignment of
   objects the slab space will be used to allocate.  ALLOCATE_BUFFER
   may be NULL in which case mmap is called.  DEALLOCATE_BUFFER may be
   NULL in which case munmap is called.  CTOR and DTOR are the slab's
   object constructor and destructor, respectivly and may be NULL if
   not required.  HOOK is passed as user data to the constructor and
   destructor.  */
#define HURD_SLAB_SPACE_INITIALIZER(TYPE, ALLOC, DEALLOC, CTOR,	\
				    DTOR, HOOK)			\
  {								\
    false,							\
    PTHREAD_MUTEX_INITIALIZER, 					\
    sizeof (TYPE),						\
    __alignof__ (TYPE),						\
    ALLOC,							\
    DEALLOC,							\
    CTOR,							\
    DTOR,							\
    HOOK							\
    /* The rest of the structure will be filled with zeros,     \
       which is good for us.  */				\
  }


/* Create a new slab space with the given object size, alignment,
   constructor and destructor.  ALIGNMENT can be zero.
   ALLOCATE_BUFFER may be NULL in which case mmap is called.
   DEALLOCATE_BUFFER may be NULL in which case munmap is called.  CTOR
   and DTOR are the slabs object constructor and destructor,
   respectivly and may be NULL if not required.  HOOK is passed as the
   first argument to the constructor and destructor.  */
error_t hurd_slab_create (size_t size, size_t alignment,
			  hurd_slab_allocate_buffer_t allocate_buffer,
			  hurd_slab_deallocate_buffer_t deallocate_buffer,
			  hurd_slab_constructor_t constructor,
			  hurd_slab_destructor_t destructor,
			  void *hook,
			  hurd_slab_space_t *space);

/* Like hurd_slab_create, but does not allocate storage for the slab.  */
error_t hurd_slab_init (hurd_slab_space_t space, size_t size, size_t alignment,
			hurd_slab_allocate_buffer_t allocate_buffer,
			hurd_slab_deallocate_buffer_t deallocate_buffer,
			hurd_slab_constructor_t constructor,
			hurd_slab_destructor_t destructor,
			void *hook);

/* Destroy all objects and the slab space SPACE.  Returns EBUSY if
   there are still allocated objects in the slab.  */
error_t hurd_slab_destroy (hurd_slab_space_t space);

/* Allocate a new object from the slab space SPACE.  */
error_t hurd_slab_alloc (hurd_slab_space_t space, void **buffer);

/* Deallocate the object BUFFER from the slab space SPACE.  */
void hurd_slab_dealloc (hurd_slab_space_t space, void *buffer);

/* Destroy all objects and the slab space SPACE.  If there were no
   outstanding allocations free the slab space.  Returns EBUSY if
   there are still allocated objects in the slab space.  */
error_t hurd_slab_free (hurd_slab_space_t space);

#endif	/* _HURD_SLAB_H */
