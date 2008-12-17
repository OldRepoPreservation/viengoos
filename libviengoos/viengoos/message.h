/* message.h - Message buffer definitions.
   Copyright (C) 2008 Free Software Foundation, Inc.
   Written by Neal H. Walfield <neal@gnu.org>.

   This file is part of the GNU Hurd.

   GNU Hurd is free software: you can redistribute it and/or modify it
   under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   GNU Hurd is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with GNU Hurd.  If not, see
   <http://www.gnu.org/licenses/>.  */

#ifndef _VIENGOOS_MESSAGE_H
#define _VIENGOOS_MESSAGE_H 1

#include <stdint.h>
#include <assert.h>
#include <viengoos/addr.h>
#include <hurd/stddef.h>

/* A message.

   When handing a message structure to a messenger, it must start at
   the beginning of a page and it cannot extend past the end of that
   page.  */
struct vg_message
{
  union
  {
    struct
    {
      /* The number of capability addresses in the message.  */
      uint16_t cap_count;
      /* The number of bytes of data transferred in this message.  */
      uint16_t data_count;

      vg_addr_t caps[/* cap_count */];
      // char data[data_count];
    };

    char raw[PAGESIZE];
  };
};


/* Clear the msg so that it references no capabilities and
   contains no data.  */
static inline void
vg_message_clear (struct vg_message *msg)
{
  msg->cap_count = 0;
  msg->data_count = 0;
}


/* Return the number of capabilities referenced by MSG.  */
static inline int
vg_message_cap_count (struct vg_message *msg)
{
  int max = (PAGESIZE - __builtin_offsetof (struct vg_message, caps))
    / sizeof (vg_addr_t);

  int count = msg->cap_count;
  if (count > max)
    count = max;
  
  return count;
}

/* Return the number of bytes of data in MSG.  */
static inline int
vg_message_data_count (struct vg_message *msg)
{
  int max = PAGESIZE
    - vg_message_cap_count (msg) * sizeof (vg_addr_t)
    - __builtin_offsetof (struct vg_message, caps);

  int count = msg->data_count;
  if (count > max)
    count = max;
  
  return count;
}


/* Return the start of the capability address array in msg MSG.  */
static inline vg_addr_t *
vg_message_caps (struct vg_message *msg)
{
  return msg->caps;
}

/* Return capability IDX in msg MSG.  */
static inline vg_addr_t
vg_message_cap (struct vg_message *msg, int idx)
{
  assert (idx < msg->cap_count);

  return msg->caps[idx];
}


/* Return the start of the data in msg MSG.  */
static inline char *
vg_message_data (struct vg_message *msg)
{
  return (void *) msg
    + __builtin_offsetof (struct vg_message, caps)
    + msg->cap_count * sizeof (vg_addr_t);
}

/* Return data word WORD in msg MSG.  */
static inline uintptr_t
vg_message_word (struct vg_message *msg, int word)
{
  assert (word < msg->data_count / sizeof (uintptr_t));

  return ((uintptr_t *) vg_message_data (msg))[word];
}


/* Append the array of capability addresses CAPS to the msg MSG.
   There must be sufficient room in the message buffer.  */
static inline void
vg_message_append_caps (struct vg_message *msg, int cap_count, vg_addr_t *caps)
{
  assert ((void *) vg_message_data (msg) - (void *) msg
	  + vg_message_data_count (msg) + cap_count * sizeof (*caps)
	  <= PAGESIZE);

  __builtin_memmove (&msg->caps[msg->cap_count + cap_count],
		     &msg->caps[msg->cap_count],
		     msg->data_count);

  __builtin_memcpy (&msg->caps[msg->cap_count],
		    caps,
		    cap_count * sizeof (vg_addr_t));

  msg->cap_count += cap_count;
}

/* Append the capability address CAP to the msg MSG.  There must be
   sufficient room in the message buffer.  */
static inline void
vg_message_append_cap (struct vg_message *msg, vg_addr_t vg_cap)
{
  vg_message_append_caps (msg, 1, &vg_cap);
}


/* Append DATA to the msg MSG.  There must be sufficient room in the
   message buffer.  */
static inline void
vg_message_append_data (struct vg_message *msg, int bytes, char *data)
{
  int dstart = __builtin_offsetof (struct vg_message, caps)
    + msg->cap_count * sizeof (vg_addr_t);
  int dend = dstart + msg->data_count;

  int new_dend = dend + bytes;
  assert (new_dend <= PAGESIZE);

  msg->data_count += bytes;
  __builtin_memcpy ((void *) msg + dend, data, bytes);
}

/* Append the word WORD to the msg MSG.  There must be
   sufficient room in the message buffer.  */
static inline void
vg_message_append_word (struct vg_message *msg, uintptr_t word)
{
  vg_message_append_data (msg, sizeof (word), (char *) &word);
}

/* Return data word WORD in msg MSG.  */
static inline void
vg_message_word_set (struct vg_message *msg, int pos, uintptr_t word)
{
  if (msg->data_count < pos * sizeof (uintptr_t))
    msg->data_count = pos * sizeof (uintptr_t);

  ((uintptr_t *) vg_message_data (msg))[pos] = word;
}

#include <s-printf.h>

static inline void
vg_message_dump (struct vg_message *message)
{
  s_printf ("%d bytes, %d caps\n",
	    vg_message_data_count (message),
	    vg_message_cap_count (message));

  char d2h[] = { '0', '1', '2', '3', '4', '5', '6', '7', '8', '9',
		 'A', 'B', 'C', 'D', 'E', 'F' };
  unsigned char *data = vg_message_data (message);

  int i = 0;
  while (i < vg_message_data_count (message))
    {
      s_printf ("%d: ", i);

      int j, k;
      for (j = 0, k = 0;
	   i < vg_message_data_count (message) && j < 4 * 8;
	   j ++, i ++)
	{
	  s_printf ("%c%c", d2h[data[i] >> 4], d2h[data[i] & 0xf]);
	  if (j % 4 == 3)
	    s_printf (" ");
	}
      s_printf ("\n");
    }

  for (i = 0; i < vg_message_cap_count (message); i ++)
    s_printf ("cap %d: " VG_ADDR_FMT "\n",
	      i, VG_ADDR_PRINTF (vg_message_cap (message, i)));
}


#endif /* _VIENGOOS_MESSAGE_H  */
