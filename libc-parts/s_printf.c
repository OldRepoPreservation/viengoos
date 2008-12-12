/* s_printf.c - Simple output routines.
   Copyright (C) 2003, 2007, 2008 Free Software Foundation, Inc.
   Written by Marcus Brinkmann.

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
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA. */

/* This implementation of printf is special in that it works in place
   and does not dynamically allocate memory.  This makes it
   appropriate for use in debugging code before malloc is functional
   or if the state is uncertain, for instance, if assert or panic is
   called.  */

#if HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdarg.h>
#include <hurd/rm.h>

#include "s-printf.h"

#ifndef RM_INTERN
static void
io_buffer_flush (struct io_buffer *buffer)
{
  if (buffer->len == 0)
    return;

  // rm_write_send_nonblocking (ADDR_VOID, ADDR_VOID, *buffer, ADDR_VOID);
  l4_msg_tag_t tag = l4_niltag;
  l4_msg_tag_set_label (&tag, 2132);

  l4_msg_t msg;
  l4_msg_clear (msg);
  l4_msg_set_msg_tag (msg, tag);

  l4_msg_append_word (msg, buffer->len);

  assert (buffer->len <= sizeof (buffer->data));

  uintptr_t *data = (uintptr_t) &buffer->data[0];
  int remaining;
  for (remaining = buffer->len; remaining > 0; remaining -= sizeof (uintptr_t))
    l4_msg_append_word (msg, *(data ++));

  l4_msg_load (msg);

  extern struct hurd_startup_data *__hurd_startup_data;      
  l4_send (__hurd_startup_data->rm);

  buffer->len = 0;
}

static void
io_buffer_append (struct io_buffer *buffer, int chr)
{
  if (buffer->len == sizeof (buffer->data))
    io_buffer_flush (buffer);

  buffer->data[buffer->len ++] = chr;
}
#endif

/* Print the single character CHR on the output device.  */
int
s_putchar (int chr)
{
#if defined(RM_INTERN) || defined(_L4_TEST_ENVIRONMENT)
  /* For Viengoos, use driver putchar routine.  For the test
     environment, just use the stdio putchar.  */
  extern int putchar (int chr);

  return putchar (chr);
#else
  struct io_buffer buffer;
  buffer.len = 1;
  buffer.data[0] = chr;
  io_buffer_flush (&buffer);
  return 0;
#endif
}

int
s_cputs (int (*putchar) (int), const char *str)
{
  while (*str != '\0')
    putchar (*(str++));

  putchar ('\n');

  return 0;
}


int
s_puts (const char *str)
{
#ifndef RM_INTERN
  struct io_buffer buffer;
  buffer.len = 0;

  int putchar (int chr)
  {
    io_buffer_append (&buffer, chr);
    return 0;
  }
#endif

  int ret = s_cputs (putchar, str);

#ifndef RM_INTERN
  io_buffer_flush (&buffer);
#endif

  return ret;
}


static void
print_nr (int (*putchar) (int), unsigned long long nr, int base)
{
  static char *digits = "0123456789abcdef";
  char str[30];
  int i = 0;

  do
    {
      str[i++] = digits[nr % base];
      nr = nr / base;
    }
  while (nr);

  i--;
  while (i >= 0)
    putchar (str[i--]);
}
  

static void
print_signed_nr (int (*putchar) (int), long long nr, int base)
{
  unsigned long long unr;

  if (nr < 0)
    {
      putchar ('-');
      unr = -nr;
    }
  else
    unr = nr;

  print_nr (putchar, unr, base);
}
  

int
s_cvprintf (int (*putchar) (int), const char *fmt, va_list ap)
{
  const char *p = fmt;

  while (*p != '\0')
    {
      if (*p != '%')
	{
	  putchar (*(p++));
	  continue;
	}

      p++;
      switch (*p)
	{
	case '%':
	  putchar ('%');
	  p++;
	  break;

	case 'l':
	  p++;
	  if (*p != 'l')
	    {
	      putchar ('%');
	      putchar ('l');
	      putchar (*(p++));
	      continue;
	    }
	  p++;
	  switch (*p)
	    {
	    case 'o':
	      print_nr (putchar, va_arg (ap, unsigned long long), 8);
	      p++;
	      break;

	    case 'd':
	    case 'i':
	      print_signed_nr (putchar, va_arg (ap, long long), 10);
	      p++;
	      break;

	    case 'x':
	    case 'X':
	      print_nr (putchar, va_arg (ap, unsigned long long), 16);
	      p++;
	      break;

	    case 'u':
	      print_nr (putchar, va_arg (ap, unsigned long long), 10);
	      p++;
	      break;

	    default:
	      putchar ('%');
	      putchar ('l');
	      putchar ('l');
	      putchar (*(p++));
	      break;
	    }
	  break;

	case 'o':
	  print_nr (putchar, va_arg (ap, unsigned int), 8);
	  p++;
	  break;

	case 'd':
	case 'i':
	  print_signed_nr (putchar, va_arg (ap, int), 10);
	  p++;
	  break;

	case 'x':
	case 'X':
	  print_nr (putchar, va_arg (ap, unsigned int), 16);
	  p++;
	  break;

	case 'u':
	  print_nr (putchar, va_arg (ap, unsigned int), 10);
	  p++;
	  break;

	case 'c':
	  putchar (va_arg (ap, int));
	  p++;
	  break;

	case 's':
	  {
	    char *str = va_arg (ap, char *);
	    if (str)
	      while (*str)
		putchar (*(str++));
	    else
	      {
		putchar ('N');
		putchar ('U');
		putchar ('L');
		putchar ('L');
	      }
	  }
	  p++;
	  break;

	case 'p':
	  print_nr (putchar, (unsigned int) va_arg (ap, void *), 16);
	  p++;
	  break;

	default:
	  putchar ('%');
	  putchar (*p);
	  p++;
	  break;
	}
    }

  return 0;
}

int
s_vprintf (const char *fmt, va_list ap)
{
#ifndef RM_INTERN
  struct io_buffer buffer;
  buffer.len = 0;

  int putchar (int chr)
  {
    io_buffer_append (&buffer, chr);
    return 0;
  }
#endif

  int ret = s_cvprintf (putchar, fmt, ap);

#ifndef RM_INTERN
  io_buffer_flush (&buffer);
#endif

  return ret;
}

int
s_cprintf (int (*putchar) (int), const char *fmt, ...)
{
  va_list ap;

  va_start (ap, fmt);
  int r = s_cvprintf (putchar, fmt, ap);
  va_end (ap);
  return r;
}

int
s_printf (const char *fmt, ...)
{
#ifndef RM_INTERN
  struct io_buffer buffer;
  buffer.len = 0;

  int putchar (int chr)
  {
    io_buffer_append (&buffer, chr);
    return 0;
  }
#endif

  va_list ap;

  va_start (ap, fmt);
  int r = s_cvprintf (putchar, fmt, ap);
  va_end (ap);

#ifndef RM_INTERN
  io_buffer_flush (&buffer);
#endif

  return r;
}
