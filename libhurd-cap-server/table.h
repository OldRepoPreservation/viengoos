/* table.h - Table abstraction interface.
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

#include <errno.h>
#include <stdlib.h>
#include <assert.h>


/* The value used for empty table entries.  */
#define HURD_TABLE_EMPTY	(NULL)


/* The type of a table entry.  */
typedef void *hurd_table_entry_t;


struct hurd_table
{
  /* The number of allocated table entries.  */
  unsigned int size;

  /* The number of table entries that are initialized.  */
  unsigned int init_size;

  /* The number of used table entries.  */
  unsigned int used;

  /* The index of the lowest entry that is unused.  */
  unsigned int first_free;

  /* The index after the highest entry that is used.  */
  unsigned int last_used;

  /* The table data.  */
  hurd_table_entry_t *data;
};
typedef struct hurd_table *hurd_table_t;


#define HURD_TABLE_INITIALIZER						\
  { .size = 0, .init_size = 0, .used = 0, .first_free = 0,		\
    .last_used = 0, .data = NULL }


/* Initialize the table TABLE.  */
error_t hurd_table_init (hurd_table_t table);


/* Destroy the table TABLE.  */
void hurd_table_destroy (hurd_table_t table);


/* Add the table element DATA to the table TABLE.  The index for this
   element is returned in R_IDX.  */
error_t hurd_table_enter (hurd_table_t table, hurd_table_entry_t data,
			  unsigned int *r_idx);


/* Lookup the table element with the index IDX in the table TABLE.  If
   there is no element with this index, return HURD_TABLE_EMPTY.  */
static inline hurd_table_entry_t
hurd_table_lookup (hurd_table_t table, unsigned int idx)
{
  error_t err;
  hurd_table_entry_t result;

  if (idx >= table->init_size)
    result = HURD_TABLE_EMPTY;
  else
    result = table->data[idx];

  return result;
}


/* Remove the table element with the index IDX from the table
   TABLE.  */
static inline void
hurd_table_remove (hurd_table_t table, unsigned int idx)
{
  assert (idx < table->init_size);
  assert (table->data[idx] != HURD_TABLE_EMPTY);

  table->data[idx] = HURD_TABLE_EMPTY;

  if (idx < table->first_free)
    table->first_free = idx;

  if (idx == table->last_used - 1)
    while (--table->last_used > 0)
      if (table->data[table->last_used - 1] == HURD_TABLE_EMPTY)
	break;

  table->used--;
}


/* Iterate over all elements in the table.  You use this macro
   with a block, for example like this:

     error_t err;
     HURD_TABLE_ITERATE (table, idx)
       {
         err = foo (idx);
         if (err)
           break;
       }
     if (err)
       cleanup_and_return ();

   Or even like this:

     HURD_TABLE_ITERATE (ht, idx)
       foo (idx);

   The block will be run for every used element in the table.  Because
   IDX is already a verified valid table index, you can lookup the
   table entry with the fast macro HURD_TABLE_LOOKUP.  */
#define HURD_TABLE_ITERATE(table, idx)					\
  for (unsigned int idx = 0; idx < (table)->init_size; idx++)		\
    if ((table)->data[idx] != HURD_TABLE_EMPTY)

/* Fast accessor for HURD_TABLE_ITERATE users.  */
#define HURD_TABLE_LOOKUP(table, idx)	((table)->data[idx])
