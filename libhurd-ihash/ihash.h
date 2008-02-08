/* ihash.h - Integer keyed hash table interface.
   Copyright (C) 1995, 2003, 2004, 2007, 2008 Free Software Foundation, Inc.
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
#include <stddef.h>
#include <limits.h>
#include <stdint.h>
#include <stdbool.h>
/* If your system does not provide __WORDSIZE, you can also try
   deriving it via <stdint.h>:

     #if UINT64_MAX == UINTPTR_MAX
     # define __WORDSIZE 64
     #else
     # define __WORDSIZE 32
     #endif

  Depending on how the constants are derived this might not work.  */
#include <bits/wordsize.h>


/* The type of the values corresponding to the keys.  Must be a
   pointer type.  The values (hurd_ihash_value_t) 0 and
   (hurd_ihash_value_t) ~0 are reserved for the implementation.  */
typedef void *hurd_ihash_value_t;

/* When an value entry in the hash table is _HURD_IHASH_EMPTY or
   _HURD_IHASH_DELETED, then the location is available, and none of
   the other members of the item are valid at that index.  The
   difference is that searches continue though _HURD_IHASH_DELETED,
   but stop at _HURD_IHASH_EMPTY.  */
#define _HURD_IHASH_EMPTY	((hurd_ihash_value_t) 0)
#define _HURD_IHASH_DELETED	((hurd_ihash_value_t) -1)

/* The type of integer we want to use for the keys.  */
typedef uintptr_t hurd_ihash_key_t;
typedef uint64_t hurd_ihash_key64_t;

/* The type of a location pointer, which is a pointer to the hash
   value stored in the hash table.  */
typedef hurd_ihash_value_t *hurd_ihash_locp_t;


/* The type of the cleanup function, which is called for every value
   removed from the hash table.  */
typedef void (*hurd_ihash_cleanup_t) (hurd_ihash_value_t value, void *arg);


struct _hurd_ihash_item
{
  /* The value of this hash item.  Must be the first element of
     the struct for the HURD_IHASH_ITERATE macro.  */
  hurd_ihash_value_t value;

  /* The integer key of this hash item.  */
  hurd_ihash_key_t key;
};
typedef struct _hurd_ihash_item *_hurd_ihash_item_t;

struct _hurd_ihash_item64
{
  /* The value of this hash item.  Must be the first element of
     the struct for the HURD_IHASH_ITERATE macro.  */
  hurd_ihash_value_t value;

  /* The integer key of this hash item.  */
  hurd_ihash_key64_t key;
};
typedef struct _hurd_ihash_item64 *_hurd_ihash_item64_t;

struct hurd_ihash
{
  /* The number of hashed elements.  */
  size_t nr_items;

#if __WORDSIZE == 32
  /* Whether items is an array consisting of _hurd_ihash_item_t or
     _hurd_ihash_item64_t elements.  */
  bool large;
# define _HURD_IHASH_LARGE(ht) ((ht)->large)
#else
  /* The machine word size is 64-bits.  */
# define _HURD_IHASH_LARGE(ht) (false)
#endif

  /* An array of (key, value) pairs (either _hurd_ihash_item_t or
     _hurd_ihash_item64_t).  */
  void *items;

  /* The length of the array ITEMS (in number of items, not bytes).  */
  size_t size;

  /* The offset of the location pointer from the hash value.  */
  intptr_t locp_offset;

  /* The maximum load factor in percent.  */
  int max_load;

  /* When freeing or overwriting an element, this function is called
     with the value as the first argument, and CLEANUP_DATA as the
     second argument.  This does not happen if CLEANUP is NULL.  */
  hurd_ihash_cleanup_t cleanup;
  void *cleanup_data;
};
typedef struct hurd_ihash *hurd_ihash_t;


/* Construction and destruction of hash tables.  */

/* The default value for the maximum load factor in percent.  */
#define HURD_IHASH_MAX_LOAD_DEFAULT 80

/* The LOCP_OFFS to use if no location pointer is available.  */
#define HURD_IHASH_NO_LOCP	PTRDIFF_MIN

/* The static initializer for a struct hurd_ihash.  */
#if __WORDSIZE == 32
#define HURD_IHASH_INITIALIZER(locp_offs, large)			\
  { .nr_items = 0, .size = 0, .cleanup = (hurd_ihash_cleanup_t) 0,	\
    .max_load = HURD_IHASH_MAX_LOAD_DEFAULT,				\
    .locp_offset = (locp_offs), large = (large) }
#else
#define HURD_IHASH_INITIALIZER(locp_offs, large)			\
  { .nr_items = 0, .size = 0, .cleanup = (hurd_ihash_cleanup_t) 0,	\
    .max_load = HURD_IHASH_MAX_LOAD_DEFAULT,				\
    .locp_offset = (locp_offs) }
#endif
/* Initialize the hash table at address HT.  LARGE determines whether
   the hash should uses 64-bit or machine word sized keys.  If
   LOCP_OFFSET is not HURD_IHASH_NO_LOCP, then this is an offset (in
   bytes) from the address of a hash value where a location pointer
   can be found.  The location pointer must be of type
   hurd_ihash_locp_t and can be used for fast removal with
   hurd_ihash_locp_remove().  This function is not provided if
   compiled with NO_MALLOC.  */
void hurd_ihash_init (hurd_ihash_t ht, bool large, intptr_t locp_offs);

/* Return the size of a buffer (in bytes) that is appropriate for a
   hash with COUNT elements, if LARGE, with 64-bit keys, otherwise
   with machine-word sized keys, and a load factor of LOAD_FACTOR
   (LOAD_FACTOR must be between 1 and 100, a load factor of 0 implies
   the default load factor).  */
size_t hurd_ihash_buffer_size (size_t count, bool large, int max_load_factor);

/* Initialize a hash ala hurd_ihash_init but provide an initial buffer
   BUFFER of size SIZE bytes.  LARGE determines whether the hash
   should uses 64-bit or machine word sized keys.  If not compiled
   with NO_MALLOC, the memory is assumed to be allocated with malloc
   and may be freed if the hash must be grown or when
   hurd_ihash_destroy is called.  */
void hurd_ihash_init_with_buffer (hurd_ihash_t ht, bool large,
				  intptr_t locp_offs,
				  void *buffer, size_t size);

/* Destroy the hash table at address HT.  This first removes all
   elements which are still in the hash table, and calling the cleanup
   function for them (if any).  If compiled with NO_MALLOC, it is the
   caller's responsibility to free the originally provided buffer,
   otherwise, any buffer in use if freed.  */
void hurd_ihash_destroy (hurd_ihash_t ht);

/* Create a hash table, initialize it and return it in HT.  LARGE
   determines whether the hash should uses 64-bit or machine word
   sized keys.  If LOCP_OFFSET is not HURD_IHASH_NO_LOCP, then this is
   an offset (in bytes) from the address of a hash value where a
   location pointer can be found.  The location pointer must be of
   type hurd_ihash_locp_t and can be used for fast removal with
   hurd_ihash_locp_remove().  If a memory allocation error occurs,
   ENOMEM is returned, otherwise 0.  This function is not provided if
   compiled with NO_MALLOC.  */
error_t hurd_ihash_create (hurd_ihash_t *ht, bool large,
			   intptr_t locp_offs);

/* Destroy the hash table HT and release the memory allocated for it
   by hurd_ihash_create().  This function is not provided if compiled
   with NO_MALLOC.  */
void hurd_ihash_free (hurd_ihash_t ht);


/* Configuration of the hash table.  */

/* Set the cleanup function for the hash table HT to CLEANUP.  The
   second argument to CLEANUP will be CLEANUP_DATA on every
   invocation.  */
void hurd_ihash_set_cleanup (hurd_ihash_t ht, hurd_ihash_cleanup_t cleanup,
			     void *cleanup_data);

/* Set the maximum load factor in percent to MAX_LOAD, which should be
   between 50 and 100.  The default is HURD_IHASH_MAX_LOAD_DEFAULT.
   New elements are only added to the hash table while the number of
   hashed elements is that much percent of the total size of the hash
   table.  If more elements are added, the hash table is first
   expanded and reorganized.  A MAX_LOAD of 100 will always fill the
   whole table before enlarging it, but note that this will increase
   the cost of operations significantly when the table is almost full.

   If the value is set to a smaller value than the current load
   factor, the next reorganization will happen when a new item is
   added to the hash table.  */
void hurd_ihash_set_max_load (hurd_ihash_t ht, unsigned int max_load);


/* Add ITEM to the hash table HT under the key KEY.  If there already
   is an item under this key and OLD_VALUE is not NULL, then stores
   the value in *OLD_VALUE.  If there already is an item under this
   key and OLD_VALUE is NULL, then calls the cleanup function (if any)
   for it before overriding the value.  If HAD_VALUE is not NULL, then
   stores whether there was already an item under this key in
   *HAD_VALUE.  If a memory allocation error occurs, ENOMEM is
   returned, otherwise 0.  */
error_t
hurd_ihash_replace (hurd_ihash_t ht, hurd_ihash_key64_t key,
		    hurd_ihash_value_t item,
		    bool *had_value, hurd_ihash_value_t *old_value);

/* Add ITEM to the hash table HT under the key KEY.  If there already
   is an item under this key, call the cleanup function (if any) for
   it before overriding the value.  If a memory allocation error
   occurs, ENOMEM is returned, otherwise 0.  */
static inline error_t
hurd_ihash_add (hurd_ihash_t ht, hurd_ihash_key64_t key,
		hurd_ihash_value_t item)
{
  return hurd_ihash_replace (ht, key, item, NULL, NULL);
}

/* Find and return the item in the hash table HT with key KEY, or NULL
   if it doesn't exist.  */
hurd_ihash_value_t hurd_ihash_find (hurd_ihash_t ht, hurd_ihash_key64_t key);

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

     HURD_IHASH_ITERATE (ht, value)
       foo (value);

   The block will be run for every element in the hash table HT.  The
   value of the current element is available in the variable VALUE
   (which is declared for you and local to the block).  */

/* The implementation of this macro is peculiar.  We want the macro to
   execute a block following its invocation, so we can only prepend
   code.  This excludes creating an outer block.  However, we must
   define two variables: The hash value variable VALUE, and the loop
   variable.

   We can define variables inside the for-loop initializer (C99), but
   we can only use one basic type to do that.  We can not use two
   for-loops, because we want a break statement inside the iterator
   block to terminate the operation.  So we must have both variables
   of the same basic type, but we can make one (or both) of them a
   pointer type.

   The pointer to the value can be used as the loop variable.  This is
   also the first element of the hash item, so we can cast the pointer
   freely between these two types.  The pointer is only dereferenced
   after the loop condition is checked (but of course the value the
   pointer pointed to must not have an influence on the condition
   result, so the comma operator is used to make sure this
   subexpression is always true).  */
#define HURD_IHASH_ITERATE(ht, val)					\
  for (hurd_ihash_value_t val,						\
         *_hurd_ihash_valuep = (ht)->items;				\
       ((void *) _hurd_ihash_valuep					\
	< (ht)->items + (ht)->size * (_HURD_IHASH_LARGE (ht)		\
				      ? sizeof (struct _hurd_ihash_item64) \
				      : sizeof (struct _hurd_ihash_item))) \
         && (val = *_hurd_ihash_valuep, 1);				\
       _hurd_ihash_valuep = (hurd_ihash_value_t *)			\
	 (_HURD_IHASH_LARGE (ht)					\
	  ? (void *) (((_hurd_ihash_item64_t) _hurd_ihash_valuep) + 1)	\
	  : (void *) (((_hurd_ihash_item_t) _hurd_ihash_valuep) + 1)))	\
    if (val != _HURD_IHASH_EMPTY && val != _HURD_IHASH_DELETED)

/* Remove the entry with the key KEY from the hash table HT.  If such
   an entry was found and removed, 1 is returned, otherwise 0.  */
int hurd_ihash_remove (hurd_ihash_t ht, hurd_ihash_key64_t key);

/* Remove from the hast table HT the entry with the location pointer
   LOCP.  That is, if the location pointer is stored in a field named
   locp in the value, pass value.locp.  This call is faster than
   hurd_ihash_remove().  */
void hurd_ihash_locp_remove (hurd_ihash_t ht, hurd_ihash_locp_t locp);

#endif	/* _HURD_IHASH_H */
