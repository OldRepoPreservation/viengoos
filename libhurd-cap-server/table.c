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

#include "table.h"


/* Initialize the table TABLE.  */
error_t
hurd_table_init (hurd_table_t table)
{
  *table = (struct hurd_table) HURD_TABLE_INITIALIZER;
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

error_t
hurd_table_enter (hurd_table_t table, void *data, unsigned int *r_idx)
{
  error_t err;
  unsigned int idx;

  if (table->used == table->size)
    {
      unsigned int size_new = table->size ? 2 * table->size : TABLE_START_SIZE;
      hurd_table_entry_t *data_new;

      data_new = realloc (table->data,
			  size_new * sizeof (hurd_table_entry_t));
      if (!data_new)
	return errno;

      table->first_free = table->size;
      table->data = data_new;
      table->size = size_new;
    }

  for (idx = table->first_free; idx < table->init_size; idx++)
    if (table->data[idx] == HURD_TABLE_EMPTY)
      break;
  table->first_free = idx;

  if (idx == table->init_size)
    table->init_size++;

  table->data[idx] = data;
  table->used++;
  *r_idx = idx;
  return 0;
}
 
