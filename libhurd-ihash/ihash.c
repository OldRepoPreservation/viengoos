/* ihash.c - Integer-keyed hash table functions.
   Copyright (C) 1993-1997, 2001, 2003 Free Software Foundation, Inc.
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

#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <limits.h>
#include <stdint.h>
#include <assert.h>

#include <hurd/ihash.h>


/* The odd prime numbers greater than twice the last and less than
   2^40 (nobody needs more than 640 GB of memory).  */
static const uint64_t ihash_sizes[] =
{
  3,
  7,
  17,
  37,
  79,
  163,
  331,
  673,
  1361,
  2729,
  5471,
  10949,
  21911,
  43853,
  87719,
  175447,
  350899,
  701819,
  1403641,
  2807303,
  5614657,
  11229331,
  22458671,
  44917381,
  89834777,
  179669557,
  359339171,
  718678369,
  1437356741,
  UINT64_C (2874713497),
  UINT64_C (5749427029),
  UINT64_C (11498854069),
  UINT64_C (22997708177),
  UINT64_C (45995416409),
  UINT64_C (91990832831),
  UINT64_C (183981665689),
  UINT64_C (367963331389),
  UINT64_C (735926662813)
};


static const unsigned int ihash_nsizes = (sizeof ihash_sizes
					  / sizeof ihash_sizes[0]);


/* Return an initial index in the hash table HT for the key KEY, to
   search for an entry.  */
#define HASH(ht, key)		((key) % (ht)->size)


/* Return the next possible index in the hash table HT for the key
   KEY, given the previous one.  */
#define REHASH(ht, key, idx)	(((idx) + (key)) % (ht)->size)


/* Return 1 if the slot with the index IDX in the hash table HT is
   empty, and 0 otherwise.  */
static inline int
index_empty (hurd_ihash_t ht, unsigned int idx)
{
  return ht->table[idx] == _HURD_IHASH_EMPTY
    || ht->table[idx] == _HURD_IHASH_DELETED;
}


/* Return 1 if the index IDX in the hash table HT is occupied by the
   element with the key KEY.  */
static inline int
index_valid (hurd_ihash_t ht, unsigned int idx, hurd_ihash_key_t key)
{
  return !index_empty (ht, idx) && ht->keys[idx] == key;
}


/* Given a hash table HT, and a key KEY, find the index in the table
   of that key.  You must subsequently check with index_valid() if the
   returned index is valid.  */
static inline int
find_index (hurd_ihash_t ht, hurd_ihash_key_t key)
{
  unsigned int idx;
  unsigned int first_idx;

  first_idx = HASH (ht, key);
  idx = first_idx;

  while (ht->table[idx] != _HURD_IHASH_EMPTY && ht->keys[idx] != key)
    {
      idx = REHASH (ht, key, idx);
      if (idx == first_idx)
	break;
    }

  return idx;
}


/* Construction and destruction of hash tables.  */

/* Initialize the hash table at address HT.  */
void
hurd_ihash_init (hurd_ihash_t ht)
{
  ht->size = 0;
  ht->cleanup = 0;
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

  if (ht->size > 0)
    {
      free (ht->table);
      free (ht->keys);
      free (ht->locps);
    }
}


/* Create a hash table, initialize it and return it in HT.  If a
   memory allocation error occurs, ENOMEM is returned, otherwise 0.  */
error_t
hurd_ihash_create (hurd_ihash_t *ht)
{
  *ht = malloc (sizeof (struct hurd_ihash));
  if (*ht == NULL)
    return ENOMEM;

  hurd_ihash_init (*ht);

  return 0;
}


/* Destroy the hash table HT and release the memory allocated for it
   by hurd_ihash_create().  */
void
hurd_ihash_free (hurd_ihash_t ht)
{
  hurd_ihash_destroy (ht);
  free (ht);
}


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


/* Helper function for hurd_ihash_add.  Return 1 if the item was
   added, and 0 if it could not be added because no empty slot was
   found.  The arguments are identical to hurd_ihash_add.  */
static inline int
add_one (hurd_ihash_t ht, hurd_ihash_key_t key,
	 hurd_ihash_value_t item, hurd_ihash_locp_t *locp)
{
  unsigned int idx;
  unsigned int first_idx;
  unsigned int first_free;

  first_idx = HASH (ht, key);
  idx = first_idx;
  first_free = first_idx;

  /* Search for for an empty or deleted space.  Even if a deleted
     space is found, keep going until we know if the same key
     already exists.  */
  while (ht->table[idx] != _HURD_IHASH_EMPTY
	 && ht->keys[idx] != key)
    {
      if (ht->table[idx] == _HURD_IHASH_DELETED)
	first_free = idx;
      
      idx = REHASH (ht, key, idx);
      if (idx == first_idx)
	break;
    }

  if (ht->keys[idx] == key)
    {
      if (ht->cleanup)
	ht->cleanup (ht->table[idx], ht->cleanup_data);
      ht->table[idx] = _HURD_IHASH_DELETED;
    }

  /* If we have not found an empty slot, maybe the last one we
     looked at was empty (or just got deleted).  */
  if (!index_empty (ht, first_free))
    first_free = idx;
 
  if (index_empty (ht, first_free))
    {
      ht->table[first_free] = item;
      ht->keys[first_free] = key;
      ht->locps[first_free] = locp;
      
      if (locp)
	*locp = &ht->table[first_free];
      
      return 1;
    }

  return 0;
}

  
/* Add ITEM to the hash table HT under the key KEY.  LOCP is the
   address of a location pointer in ITEM; If non-NULL, it will be
   filled with a pointer that may be used as an argument to
   hurd_ihash_locp_remove().  (The variable pointed to by LOCP may be
   written to subsequently between this call and when the element is
   deleted).  If there already is an item under this key, call the
   cleanup function (if any) for it before overriding the value.  If a
   memory allocation error occurs, ENOMEM is returned, otherwise 0.  */
error_t
hurd_ihash_add (hurd_ihash_t ht, hurd_ihash_key_t key,
		hurd_ihash_value_t item, hurd_ihash_locp_t *locp)
{
  struct hurd_ihash old_ht = *ht;
  int was_added;
  int i;

  if (ht->size && add_one (ht, key, item, locp))
    return 0;

  /* The hash table is too small, and we have to increase it.  */
  for (i = 0; i < ihash_nsizes; i++)
    if (ihash_sizes[i] > old_ht.size)
      break;
  if (i == ihash_nsizes
      || ihash_sizes[i] > SIZE_MAX / sizeof (hurd_ihash_value_t)
      || ihash_sizes[i] > SIZE_MAX / sizeof (hurd_ihash_key_t)
      || ihash_sizes[i] > SIZE_MAX / sizeof (hurd_ihash_locp_t *))
    return ENOMEM;		/* Surely will be true momentarily.  */
    
  ht->size = ihash_sizes[i];
  ht->table = malloc (ht->size * sizeof (hurd_ihash_value_t));
  ht->keys = malloc (ht->size * sizeof (hurd_ihash_key_t));
  ht->locps = malloc (ht->size * sizeof (hurd_ihash_locp_t *));

  if (ht->table == NULL || ht->locps == NULL || ht->keys == NULL)
    {
      if (ht->table)
	free (ht->table);
      if (ht->keys)
	free(ht->keys);
      if (ht->locps)
	free (ht->locps);

      *ht = old_ht;
      return ENOMEM;
    }

  for (i = 0; i < ht->size; i++)
    ht->table[i] = _HURD_IHASH_EMPTY;

  /* We have to rehash the old entries.  */
  for (i = 0; i < old_ht.size; i++)
    if (!index_empty (&old_ht, i))
      {
	was_added = add_one (ht, old_ht.keys[i], old_ht.table[i],
			     old_ht.locps[i]);
	assert (was_added);
      }

  /* Finally add the new element!  */
  was_added = add_one (ht, key, item, locp);
  assert (was_added);

  if (old_ht.size > 0)
    {
      free (old_ht.table);
      free (old_ht.keys);
      free (old_ht.locps);
    }

  return 0;
}


/* Find and return the item in the hash table HT with key KEY, or NULL
   if it doesn't exist.  */
hurd_ihash_value_t
hurd_ihash_find (hurd_ihash_t ht, hurd_ihash_key_t key)
{
  if (ht->size == 0)
    return NULL;
  else
    {
      int idx = find_index (ht, key);
      return index_valid (ht, idx, key) ? ht->table[idx] : NULL;
    }
}


/* Remove the entry with the key KEY from the hash table HT.  If such
   an entry was found and removed, 1 is returned, otherwise 0.  */
int
hurd_ihash_remove (hurd_ihash_t ht, hurd_ihash_key_t key)
{
  int idx = find_index (ht, key);

  if (index_valid (ht, idx, key))
    {
      hurd_ihash_locp_remove (ht, &ht->table[idx]);
      return 1;
    }
  else
    return 0;
}


/* Remove the entry pointed to by the location pointer LOCP from the
   hashtable HT.  LOCP is the location pointer of which the address
   was provided to hurd_ihash_add().  This call is faster than
   hurd_ihash_remove().  HT can be NULL, in which case the call still
   succeeds, but the cleanup function (if any) will not be invoked in
   this case.  */
void
hurd_ihash_locp_remove (hurd_ihash_t ht, hurd_ihash_locp_t locp)
{
  if (ht && ht->cleanup)
    (*ht->cleanup) (*locp, ht->cleanup_data);
  *locp = _HURD_IHASH_DELETED;
}
