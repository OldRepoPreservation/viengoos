/* table.c - Table abstraction implementation.
   Copyright (C) 2004 Free Software Foundation, Inc.
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

#if HAVE_CONFIG_H
#include <config.h>
#endif

#include <assert.h>
#include <string.h>

#include "table.h"


/* Initialize the table TABLE.  */
error_t
hurd_table_init (hurd_table_t table, unsigned int entry_size)
{
  assert (sizeof (entry_size) >= sizeof (void *));

  *table = (struct hurd_table) HURD_TABLE_INITIALIZER (entry_size);
}


/* Destroy the table TABLE.  */
void
hurd_table_destroy (hurd_table_t table)
{
  if (table->data)
    free (table->data);
}


/* The initial table size.  */
#define TABLE_START_SIZE	4

/* Add the table element DATA to the table TABLE.  The index for this
   element is returned in R_IDX.  Note that the data is added by
   copying ENTRY_SIZE bytes into the table (the ENTRY_SIZE parameter
   was provided at table initialization time).  */
error_t
hurd_table_enter (hurd_table_t table, void *data, unsigned int *r_idx)
{
  error_t err;
  unsigned int idx;

  if (table->used == table->size)
    {
      unsigned int size_new = table->size ? 2 * table->size : TABLE_START_SIZE;
      void *data_new;

      data_new = realloc (table->data, size_new * table->entry_size);
      if (!data_new)
	return errno;

      table->first_free = table->size;
      table->data = data_new;
      table->size = size_new;
    }

  for (idx = table->first_free; idx < table->init_size; idx++)
    if (_HURD_TABLE_ENTRY_LOOKUP (table, idx) == HURD_TABLE_EMPTY)
      break;

  /* The following setting for FIRST_FREE is safe, because if this was
     the last table entry, then the table is full and we will grow the
     table the next time we are called (if no elements are removed in
     the meantime.  */
  table->first_free = idx + 1;

  if (idx == table->init_size)
    table->init_size++;

  memcpy (HURD_TABLE_LOOKUP (table, idx), data, table->entry_size);
  table->used++;
  *r_idx = idx;
  return 0;
}
 
