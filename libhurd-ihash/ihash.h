typedef int error_t;
/* ihash.h - Integer keyed hash table interface.
   Copyright (C) 1995, 2003 Free Software Foundation, Inc.
   Written by Miles Bader <miles@gnu.org>.
   Revised by Marcus Brinkmann <marcus@gnu.org>.

   This file is part of the GNU Hurd.

   The GNU Hurd is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2, or (at
   your option) any later version.

   The GNU Hurd is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with the GNU Hurd; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.  */

#ifndef _HURD_IHASH_H
#define _HURD_IHASH_H	1

#include <errno.h>
#include <sys/types.h>


/* The type of the values corresponding to the keys.  Must be a
   pointer type.  */
typedef void *hurd_ihash_value_t;

/* When an entry in the table TABLE of a hash table is
   _HURD_IHASH_EMPTY or _HURD_IHASH_DELETED, then the location is
   available, and none of the other arrays are valid at that index.
   The difference is that searches continue though HASH_DEL, but stop
   at _HURD_IHASH_EMPTY.  */
#define _HURD_IHASH_EMPTY	((hurd_ihash_value_t) 0)
#define _HURD_IHASH_DELETED     	((hurd_ihash_value_t) -1)


/* The type of integer we want to use for the keys.  */
typedef unsigned int hurd_ihash_key_t;


/* The type of a location pointer, which is a pointer to the hash
   value stored in the hash table.  */
typedef hurd_ihash_value_t *hurd_ihash_locp_t;


/* The type of the cleanup function, which is called for every value
   removed from the hash table.  */
typedef void *(*hurd_ihash_cleanup_t) (hurd_ihash_value_t value, void *arg);


struct hurd_ihash
{
  /* An array storing the elements in the hash table.  */
  hurd_ihash_value_t *table;

  /* An array storing the integer key for each element.  */
  hurd_ihash_key_t *keys;

  /* An array storing pointers to the location pointers for each
     element.  These are used as cookies for quick'n'easy removal.  */
  hurd_ihash_locp_t **locps;

  /* The length of the three arrays TABLE, KEYS and LOCPS.  */
  size_t size;

  /* When freeing or overwriting an element, this function is called
     with the value as the first argument, and CLEANUP_DATA as the
     second argument.  This does not happen if CLEANUP is NULL.  */
  hurd_ihash_cleanup_t cleanup;
  void *cleanup_data;
};
typedef struct hurd_ihash *hurd_ihash_t;


/* Construction and destruction of hash tables.  */

/* Initialize the hash table at address HT.  */
void hurd_ihash_init (hurd_ihash_t ht);


/* Destroy the hash table at address HT.  This first removes all
   elements which are still in the hash table, and calling the cleanup
   function for them (if any).  */
void hurd_ihash_destroy (hurd_ihash_t ht);


/* Create a hash table, initialize it and return it in HT.  If a
   memory allocation error occurs, ENOMEM is returned, otherwise 0.  */
error_t hurd_ihash_create (hurd_ihash_t *ht);


/* Destroy the hash table HT and release the memory allocated for it
   by hurd_ihash_create().  */
void hurd_ihash_free (hurd_ihash_t ht);


/* Set the cleanup function for the hash table HT to CLEANUP.  The
   second argument to CLEANUP will be CLEANUP_DATA on every
   invocation.  */
void hurd_ihash_set_cleanup (hurd_ihash_t ht, hurd_ihash_cleanup_t cleanup,
			     void *cleanup_data);


/* Add ITEM to the hash table HT under the key KEY.  LOCP is the
   address of a location pointer in ITEM; If non-NULL, it will be
   filled with a pointer that may be used as an argument to
   hurd_ihash_locp_remove().  (The variable pointed to by LOCP may be
   written to subsequently between this call and when the element is
   deleted).  If there already is an item under this key, call the
   cleanup function (if any) for it before overriding the value.  If a
   memory allocation error occurs, ENOMEM is returned, otherwise 0.  */
error_t hurd_ihash_add (hurd_ihash_t ht, hurd_ihash_key_t key,
			hurd_ihash_value_t item, hurd_ihash_locp_t *locp);


/* Find and return the item in the hash table HT with key KEY, or NULL
   if it doesn't exist.  */
hurd_ihash_value_t hurd_ihash_find (hurd_ihash_t ht, hurd_ihash_key_t key);


/* Iterate over all elements in the hash table.  You use this macro
   with a block, for example like this:

     error_t err;
     HURD_IHASH_ITERATE (ht, value)
       {
         err = foo (value);
         if (err)
           break;
       }
     if (err)
       cleanup_and_return ();

   Or even like this:

     hurd_ihash_iterate (ht, value)
       foo (value);

   The block will be run for every element in the hash table HT.  The
   value of the current element is available via the macro
   HURD_IHASH_ITERATOR_VALUE.  */
#define HURD_IHASH_ITERATE(ht, value)					\
  for (hurd_ihash_value_t value = (ht)->table[0],			\
                          *_hurd_ihash_valuep = &(ht)->table[0];	\
       _hurd_ihash_valuep - &(ht)->table[0] < (ht)->size		\
         && (value = *_hurd_ihash_valuep, 1);				\
       _hurd_ihash_valuep++)						\
    if (value != _HURD_IHASH_EMPTY && value != _HURD_IHASH_DELETED)


/* Remove the entry with the key KEY from the hash table HT.  If such
   an entry was found and removed, 1 is returned, otherwise 0.  */
int hurd_ihash_remove (hurd_ihash_t ht, hurd_ihash_key_t key);


/* Remove the entry pointed to by the location pointer LOCP from the
   hashtable HT.  LOCP is the location pointer of which the address
   was provided to hurd_ihash_add().  This call is faster than
   hurd_ihash_remove().  HT can be NULL, in which case the call still
   succeeds, but the cleanup function (if any) will not be invoked in
   this case.  */
void hurd_ihash_locp_remove (hurd_ihash_t ht, hurd_ihash_locp_t locp);

#endif	/* _HURD_IHASH_H */
