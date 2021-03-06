/* ihash.c - Integer-keyed hash table functions.
   Copyright (C) 1993-1997, 2001, 2003, 2004, 2007, 2008 Free Software Foundation, Inc.
   Written by Michael I. Bushnell.
   Revised by Miles Bader <miles@gnu.org>.
   Revised by Marcus Brinkmann <marcus@gnu.org>.
   
   This file is part of the GNU Hurd.

   The GNU Hurd is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   The GNU Hurd is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with the GNU Hurd; see the file COPYING.  If not, write to
   the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.  */

#if HAVE_CONFIG_H
#include <config.h>
#endif

#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <limits.h>
#include <stdint.h>
#include <assert.h>

#include "ihash.h"


/* The prime numbers of the form 4 * i + 3 for some i, all greater
   than twice the previous one and smaller than 2^40 (for now).  */
static const uint64_t ihash_sizes[] =
{
  3,
  7,
  19,
  43,
  103,
  211,
  431,
  863,
  1747,
  3499,
  7019,
  14051,
  28111,
  56239,
  112507,
  225023,
  450067,
  900139,
  1800311,
  3600659,
  7201351,
  14402743,
  28805519,
  57611039,
  115222091,
  230444239,
  460888499,
  921777067,
  1843554151,
  UINT64_C (3687108307),
  UINT64_C (7374216631),
  UINT64_C (14748433279),
  UINT64_C (29496866579),
  UINT64_C (58993733159),
  UINT64_C (117987466379),
  UINT64_C (235974932759),
  UINT64_C (471949865531),
  UINT64_C (943899731087)
};

static const unsigned int ihash_nsizes = (sizeof ihash_sizes
					  / sizeof ihash_sizes[0]);


#define ITEM(ht, idx)						\
  (_HURD_IHASH_LARGE (ht)					\
   ? (void *) &((_hurd_ihash_item64_t) (ht)->items)[idx]	\
   : (void *) &((_hurd_ihash_item_t) (ht)->items)[idx])

#define VALUE(ht, idx)					\
  (_HURD_IHASH_LARGE (ht)				\
   ? ((_hurd_ihash_item64_t) ITEM (ht, idx))->value	\
   : ((_hurd_ihash_item_t) ITEM (ht, idx))->value)

#define KEY(ht, idx)					\
  (_HURD_IHASH_LARGE (ht)				\
   ? ((_hurd_ihash_item64_t) ITEM (ht, idx))->key	\
   : ((_hurd_ihash_item_t) ITEM (ht, idx))->key)

#define ITEM_SIZE(large)						\
  ((large)								\
   ? sizeof (struct _hurd_ihash_item64)					\
   : sizeof (struct _hurd_ihash_item))

/* Return 1 if the slot with the index IDX in the hash table HT is
   empty, and 0 otherwise.  */
static inline int
index_empty (hurd_ihash_t ht, unsigned int idx)
{
  return VALUE (ht, idx) == _HURD_IHASH_EMPTY
    || VALUE (ht, idx) == _HURD_IHASH_DELETED;
}


/* Return 1 if the index IDX in the hash table HT is occupied by the
   element with the key KEY.  */
static inline int
index_valid (hurd_ihash_t ht, unsigned int idx, hurd_ihash_key64_t key)
{
  return !index_empty (ht, idx) && KEY (ht, idx) == key;
}


/* Given a hash table HT, and a key KEY, find the index in the table
   of that key.  You must subsequently check with index_valid() if the
   returned index is valid.  */
static inline int
find_index (hurd_ihash_t ht, hurd_ihash_key64_t key)
{
  unsigned int idx;
  unsigned int i;
  unsigned int up_idx;
  unsigned int down_idx;

  idx = key % ht->size;

  if (VALUE (ht, idx) == _HURD_IHASH_EMPTY || KEY (ht, idx) == key)
    return idx;

  /* Instead of calculating idx + 1, idx + 4, idx + 9, ..., idx + i^2,
     we add 1, 3, 5, 7, etc to the previous index.  We do this in both
     directions separately.  */
  i = 1;
  up_idx = idx;
  down_idx = idx;

  do
    {
      up_idx = (up_idx + i) % ht->size;
      if (VALUE (ht, up_idx) == _HURD_IHASH_EMPTY
	  || KEY (ht, up_idx) == key)
	return up_idx;

      if (down_idx < i)
	down_idx += ht->size;
      down_idx = (down_idx - i) % ht->size;
      if (VALUE (ht, down_idx) == _HURD_IHASH_EMPTY
	  || KEY (ht, down_idx) == key)
	return down_idx;

      /* After (ht->size - 1) / 2 iterations, this will be 0.  */
      i = (i + 2) % ht->size;
    }
  while (i);

  /* If we end up here, the item could not be found.  Return any
     invalid index.  */
  return idx;
}


/* Remove the entry pointed to by the location pointer LOCP from the
   hashtable HT.  LOCP is the location pointer of which the address
   was provided to hurd_ihash_add().  If CLEANUP is true, call the
   cleanup handler, if any.  */
static inline void
locp_remove (hurd_ihash_t ht, hurd_ihash_locp_t locp, bool cleanup)
{
  if (cleanup && ht->cleanup)
    (*ht->cleanup) (*locp, ht->cleanup_data);
  *locp = _HURD_IHASH_DELETED;
  ht->nr_items--;
}


/* Construction and destruction of hash tables.  */

/* Initialize the hash table at address HT.  */
static void
hurd_ihash_init_internal (hurd_ihash_t ht, bool large, intptr_t locp_offs)
{
#if __WORDSIZE == 32
  ht->large = large;
#endif
  ht->nr_items = 0;
  ht->size = 0;
  ht->locp_offset = locp_offs;
  ht->max_load = HURD_IHASH_MAX_LOAD_DEFAULT;
  ht->cleanup = 0;
}

#ifndef NO_MALLOC
void
hurd_ihash_init (hurd_ihash_t ht, bool large, intptr_t locp_offs)
{
  hurd_ihash_init_internal (ht, large, locp_offs);
}
#endif

void
hurd_ihash_init_with_buffer (hurd_ihash_t ht, bool large,
			     intptr_t locp_offs,
			     void *buffer, size_t size)
{
  hurd_ihash_init_internal (ht, large, locp_offs);
  ht->items = buffer;

  int max_size = size / ITEM_SIZE (_HURD_IHASH_LARGE (ht));

  int i;
  for (i = 0; i < ihash_nsizes; i ++)
    if (ihash_sizes[i] > max_size)
      break;
  ht->size = ihash_sizes[i - 1];
}


/* Destroy the hash table at address HT.  This first removes all
   elements which are still in the hash table, and calling the cleanup
   function for them (if any).  */
void
hurd_ihash_destroy (hurd_ihash_t ht)
{
  if (ht->cleanup)
    {
      hurd_ihash_cleanup_t cleanup = ht->cleanup;
      void *cleanup_data = ht->cleanup_data;

      HURD_IHASH_ITERATE (ht, value)
	(*cleanup) (value, cleanup_data);
    }

#ifndef NO_MALLOC
  if (ht->size > 0)
    free (ht->items);
#endif
}


/* Create a hash table, initialize it and return it in HT.  If a
   memory allocation error occurs, ENOMEM is returned, otherwise 0.  */
#ifndef NO_MALLOC
error_t
hurd_ihash_create (hurd_ihash_t *ht, bool large, intptr_t locp_offs)
{
  *ht = malloc (sizeof (struct hurd_ihash));
  if (*ht == NULL)
    return ENOMEM;

  hurd_ihash_init (*ht, large, locp_offs);

  return 0;
}
#endif


/* Destroy the hash table HT and release the memory allocated for it
   by hurd_ihash_create().  */
#ifndef NO_MALLOC
void
hurd_ihash_free (hurd_ihash_t ht)
{
  hurd_ihash_destroy (ht);
  free (ht);
}
#endif


/* Set the cleanup function for the hash table HT to CLEANUP.  The
   second argument to CLEANUP will be CLEANUP_DATA on every
   invocation.  */
void
hurd_ihash_set_cleanup (hurd_ihash_t ht, hurd_ihash_cleanup_t cleanup,
			void *cleanup_data)
{
  ht->cleanup = cleanup;
  ht->cleanup_data = cleanup_data;
}


/* Set the maximum load factor in percent to MAX_LOAD, which should be
   between 1 and 100.  The default is HURD_IHASH_MAX_LOAD_DEFAULT.
   New elements are only added to the hash table while the number of
   hashed elements is that much percent of the total size of the hash
   table.  If more elements are added, the hash table is first
   expanded and reorganized.  A MAX_LOAD of 100 will always fill the
   whole table before enlarging it, but note that this will increase
   the cost of operations significantly when the table is almost full.

   If the value is set to a smaller value than the current load
   factor, the next reorganization will happen when a new item is
   added to the hash table.  */
void
hurd_ihash_set_max_load (hurd_ihash_t ht, unsigned int max_load)
{
  ht->max_load = max_load;
}


/* Helper function for hurd_ihash_replace.  Return 1 if the item was
   added, and 0 if it could not be added because no empty slot was
   found.  The arguments are identical to hurd_ihash_replace.

   We are using open address hashing.  As the hash function we use the
   division method with quadratic probe.  This is guaranteed to try
   all slots in the hash table if the prime number is 3 mod 4.  */
static inline int
replace_one (hurd_ihash_t ht, hurd_ihash_key64_t key, hurd_ihash_value_t value,
	     bool *had_value, hurd_ihash_value_t *old_value)
{
  unsigned int idx;
  unsigned int first_free;

  idx = key % ht->size;
  first_free = idx;

  if (VALUE (ht, idx) != _HURD_IHASH_EMPTY && KEY (ht, idx) != key)
    {
      /* Instead of calculating idx + 1, idx + 4, idx + 9, ..., idx +
         i^2, we add 1, 3, 5, 7, ... 2 * i - 1 to the previous index.
         We do this in both directions separately.  */
      unsigned int i = 1;
      unsigned int up_idx = idx;
      unsigned int down_idx = idx;
 
      do
	{
	  up_idx = (up_idx + i) % ht->size;
	  if (VALUE (ht, up_idx) == _HURD_IHASH_EMPTY
	      || KEY (ht, up_idx) == key)
	    {
	      idx = up_idx;
	      break;
	    }
	  if (first_free == idx
	      && VALUE (ht, up_idx) == _HURD_IHASH_DELETED)
	    first_free = up_idx;

	  if (down_idx < i)
	    down_idx += ht->size;
	  down_idx = (down_idx - i) % ht->size;
	  if (down_idx < 0)
	    down_idx += ht->size;
	  else
	    down_idx %= ht->size;
	  if (VALUE (ht, down_idx) == _HURD_IHASH_EMPTY
	      || KEY (ht, down_idx) == key)
	    {
	      idx = down_idx;
	      break;
	    }
	  if (first_free == idx
	      && VALUE (ht, down_idx) == _HURD_IHASH_DELETED)
	    first_free = down_idx;

	  /* After (ht->size - 1) / 2 iterations, this will be 0.  */
	  i = (i + 2) % ht->size;
	}
      while (i);
    }

  /* Remove the old entry for this key if necessary.  */
  if (index_valid (ht, idx, key))
    {
      if (had_value)
	*had_value = true;

      if (old_value)
	*old_value = VALUE (ht, idx);
      locp_remove (ht, ITEM (ht, idx), !! old_value);
    }
  else
    {
      if (had_value)
	*had_value = false;
    }

  /* If we have not found an empty slot, maybe the last one we
     looked at was empty (or just got deleted).  */
  if (!index_empty (ht, first_free))
    first_free = idx;
 
  if (index_empty (ht, first_free))
    {
      ht->nr_items++;

      if (_HURD_IHASH_LARGE (ht))
	{
	  _hurd_ihash_item64_t i = ITEM (ht, first_free);
	  i->value = value;
	  i->key = key;

	  if (ht->locp_offset != HURD_IHASH_NO_LOCP)
	    *((hurd_ihash_locp_t) (((char *) value) + ht->locp_offset))
	      = &i->value;
	}
      else
	{
	  _hurd_ihash_item_t i = ITEM (ht, first_free);
	  i->value = value;
	  i->key = key;

	  if (ht->locp_offset != HURD_IHASH_NO_LOCP)
	    *((hurd_ihash_locp_t) (((char *) value) + ht->locp_offset))
	      = &i->value;
	}

      return 1;
    }

  return 0;
}

/* Return the size of a buffer (in bytes) that is appropriate for a
   hash with COUNT elements and a load factor of LOAD_FACTOR
   (LOAD_FACTOR must be between 1 and 100, a load factor of 0 implies
   the default load factor).  */
size_t
hurd_ihash_buffer_size (size_t count, bool large, int max_load_factor)
{
  if (max_load_factor == 0)
    max_load_factor = HURD_IHASH_MAX_LOAD_DEFAULT;
  if (max_load_factor > 100)
    max_load_factor = 100;
  if (max_load_factor < 0)
    max_load_factor = 1;

  count = count * 100 / max_load_factor;

  int i;
  for (i = 0; i < ihash_nsizes; i++)
    if (ihash_sizes[i] >= count)
      break;
  if (i == ihash_nsizes)
    return SIZE_MAX;		/* Surely will be true momentarily.  */

  return ihash_sizes[i] * ITEM_SIZE (large);
}

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
		    bool *had_value, hurd_ihash_value_t  *old_value)
{
  if (ht->size)
    {
      /* Only fill the hash table up to its maximum load factor.  */
#ifndef NO_MALLOC
      if (ht->nr_items * 100 / ht->size <= ht->max_load)
#endif
	if (replace_one (ht, key, item, had_value, old_value))
	  return 0;
    }

#ifdef NO_MALLOC
  return ENOMEM;
#else
  struct hurd_ihash old_ht = *ht;
  int was_added;
  int i;

  /* The hash table is too small, and we have to increase it.  */
  size_t size = hurd_ihash_buffer_size (old_ht.size + 1,
					_HURD_IHASH_LARGE (ht),
					ht->max_load);
  if (size >= SIZE_MAX)
    return ENOMEM;		/* Surely will be true momentarily.  */

  ht->nr_items = 0;
  ht->size = size / ITEM_SIZE (_HURD_IHASH_LARGE (ht));
  /* calloc() will initialize all values to _HURD_IHASH_EMPTY implicitely.  */
  ht->items = calloc (ht->size, ITEM_SIZE (_HURD_IHASH_LARGE (ht)));

  if (ht->items == NULL)
    {
      if (ht->items)
	free(ht->items);

      *ht = old_ht;
      return ENOMEM;
    }

  /* We have to rehash the old entries.  */
  for (i = 0; i < old_ht.size; i++)
    if (!index_empty (&old_ht, i))
      {
	was_added = replace_one (ht, KEY (&old_ht, i), VALUE (&old_ht, i),
				 had_value, old_value);
	assert (was_added);
      }

  /* Finally add the new element!  */
  was_added = replace_one (ht, key, item, had_value, old_value);
  assert (was_added);

  if (old_ht.size > 0)
    free (old_ht.items);

  return 0;
#endif
}


/* Find and return the item in the hash table HT with key KEY, or NULL
   if it doesn't exist.  */
hurd_ihash_value_t
hurd_ihash_find (hurd_ihash_t ht, hurd_ihash_key64_t key)
{
  if (ht->size == 0)
    return NULL;
  else
    {
      int idx = find_index (ht, key);
      return index_valid (ht, idx, key) ? VALUE (ht, idx) : NULL;
    }
}


/* Remove the entry with the key KEY from the hash table HT.  If such
   an entry was found and removed, 1 is returned, otherwise 0.  */
int
hurd_ihash_remove (hurd_ihash_t ht, hurd_ihash_key64_t key)
{
  if (ht->size != 0)
    {
      int idx = find_index (ht, key);
      
      if (index_valid (ht, idx, key))
	{
	  locp_remove (ht, ITEM (ht, idx), true);
	  return 1;
	}
    }

  return 0;
}


/* Remove the entry pointed to by the location pointer LOCP from the
   hashtable HT.  LOCP is the location pointer of which the address
   was provided to hurd_ihash_add().  This call is faster than
   hurd_ihash_remove().  */
void
hurd_ihash_locp_remove (hurd_ihash_t ht, hurd_ihash_locp_t locp)
{
  locp_remove (ht, locp, true);
}
