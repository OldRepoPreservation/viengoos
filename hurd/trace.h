/* trace.c - Tracing helper functions.
   Copyright (C) 2008 Free Software Foundation, Inc.
   Written by Neal H. Walfield <neal@gnu.org>.

   This file is part of the GNU Hurd.

   GNU Hurd is free software: you can redistribute it and/or modify it
   under the terms of the GNU Lesser General Public License as
   published by the Free Software Foundation, either version 3 of the
   License, or (at your option) any later version.

   GNU Hurd is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with GNU Hurd.  If not, see
   <http://www.gnu.org/licenses/>.  */

#ifndef _HURD_TRACE_H
#define _HURD_TRACE_H

#include <stdarg.h>
#include <assert.h>

#ifdef RM_INTERN
#  include "../viengoos/mutex.h"
#else
#  include <hurd/mutex.h>
#endif

#include <s-printf.h>

struct trace_buffer
{
  /* A descriptive name for the trace buffer.  */
  char *name;
  void *id;

  bool nobacktrace;
  bool notid;
  bool nolock;

  ss_mutex_t lock;

  /* Number of bytes written to the buffer so far.  */
  int written;
  /* Current offset in the buffer.  */
  int offset;

  /* 2 pages.  */
  char buffer[2 * 4096];

  /* A couple of local variables.  This removes the necessity to
     allocate them on the stack, which is a bit problematic as we'd
     like to use this to debug exactly those cases where faulting on a
     stack results in an exception.  */
  void *bt[20];
  int count;
};

#define TRACE_BUFFER_INIT(name, id, save_backtrace, print_tid, do_lock) \
  { (name), (id), ! (save_backtrace), ! (print_tid), ! (do_lock) }

static inline void
trace_buffer_add (const char *func, const int lineno,
		  struct trace_buffer *buffer, char *fmt, ...)
{
  int pc (int chr)
  {
    assert (buffer->offset < sizeof (buffer->buffer));

    buffer->buffer[buffer->offset ++] = chr;
    if (buffer->offset == sizeof (buffer->buffer))
      buffer->offset = 0;

    buffer->written ++;

    return chr;
  }

  va_list ap;

  if (! buffer->nolock)
    ss_mutex_lock (&buffer->lock);

  if (! buffer->notid)
    {
#ifdef USE_L4
      s_cprintf (pc, "%x:", l4_myself ());
#elif !defined (RM_INTERN)
      s_cprintf (pc, "%x:", hurd_myself ());
#else
# warning Don't know how to get tid.
#endif
    }
  s_cprintf (pc, "%s:%d: ", func, lineno);

  va_start (ap, fmt);
  s_cvprintf (pc, fmt, ap);
  va_end (ap);

  /* Include a backtrace.  */

  if (! buffer->nobacktrace)
    {
      extern int backtrace (void **array, int size);
      buffer->count = backtrace (buffer->bt,
				 (sizeof (buffer->bt)
				  / sizeof (buffer->bt[0])));
      int i;
      pc (' '); pc ('(');
      for (i = 0; i < buffer->count; i ++)
	{
	  s_cprintf (pc, "%x", buffer->bt[i]);
	  if (i != buffer->count - 1)
	    pc (' ');
	}
      pc (')');
    }

  /* And add a terminating NUL.  */
  pc (0);

  if (! buffer->nolock)
    ss_mutex_unlock (&buffer->lock);
}
#define trace_buffer_add(buf, fmt, ...)					\
  trace_buffer_add (__FUNCTION__, __LINE__, buf, fmt, ##__VA_ARGS__)

static inline void
trace_buffer_dump (const char *func, int line,
		   struct trace_buffer *buffer, int count)
{
  if (! buffer->nolock)
    ss_mutex_lock (&buffer->lock);

  s_printf ("%s:%d: Dumping trace buffer %s(%x)\n",
	    func, line, buffer->name, buffer->id);

  if (buffer->written == 0)
    {
      s_puts ("Empty.\n");
      if (! buffer->nolock)
	ss_mutex_unlock (&buffer->lock);
      return;
    }

  int record = 1;

  int offset = buffer->offset - 1;
  if (offset == -1)
    offset = sizeof (buffer->buffer) - 1;

  /* Number of bytes processed.  (-1 as we increment for a byte that
     we have not actually processed.)  */
  int processed = 0;

  do
    {
      /* OFFSET is at the end of the next string.  */

      /* Last byte is a NUL character.  */
      assert (buffer->buffer[offset] == 0);

      /* Find end of the preceding string.  */
      do
	{
	  offset --, processed ++;
	  if (processed > sizeof (buffer->buffer))
	    {
	      if (! buffer->nolock)
		ss_mutex_unlock (&buffer->lock);
	      return;
	    }
	  if (processed >= buffer->written)
	    break;

	  if (offset == -1)
	    offset = sizeof (buffer->buffer) - 1;
	}
      while (buffer->buffer[offset] != 0);

      /* Offset is just in front of the last string.  */

      /* Print the string.  */
      s_printf ("%d: ", record);

      int pos = offset + 1;
      if (pos == sizeof (buffer->buffer))
	pos = 0;
      do
	{
	  s_putchar (buffer->buffer[pos ++]);

	  if (pos == sizeof (buffer->buffer))
	    pos = 0;
	}
      while (buffer->buffer[pos] != 0);

      s_putchar ('\n');

      if (record == count)
	break;

      record ++;
    }
  while (processed < buffer->written);

  if (! buffer->nolock)
    ss_mutex_unlock (&buffer->lock);
}
#define trace_buffer_dump(buffer, count)	\
  trace_buffer_dump (__FUNCTION__, __LINE__, buffer, count)

#endif /* _HURD_TRACE_H  */
