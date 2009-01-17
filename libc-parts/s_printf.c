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
#include <viengoos/misc.h>

#include "s-printf.h"

#ifndef RM_INTERN
static void
io_buffer_flush (struct io_buffer *buffer)
{
  if (buffer->len == 0)
    return;

  // rm_write_send_nonblocking (VG_ADDR_VOID, VG_ADDR_VOID, *buffer, VG_ADDR_VOID);
#ifdef USE_L4
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
#else
# warning Not ported to this platform.
  assert (0);
#endif

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

/* Output format flags.  */
#define FLAG_LONGLONG	(1 << 0)
#define FLAG_PAD_ZERO	(1 << 1)
#define FLAG_BASE	(1 << 2)
#define FLAG_UPPERCASE	(1 << 3)
#define FLAG_LONG       (1 << 4)

static void
print_nr (int (*putchar) (int),
	  unsigned long long nr, int base, int precision, unsigned int flags)
{
  static char *digits;
  char str[30];
  int i = 0;

  digits = (flags & FLAG_UPPERCASE) ? "0123456789ABCDEF" : "0123456789abcdef";

  if (flags & FLAG_BASE)
    {
      if (base == 16)
	{
	  putchar ('0');
	  putchar ((flags & FLAG_UPPERCASE) ? 'X' : 'x');
	}
      else if (base == 8)
	putchar ('0');
    }

  do
    {
      str[i++] = digits[nr % base];
      nr = nr / base;
    }
  while (nr);

  if (precision >= sizeof (str))
    precision = sizeof (str);
      
  while (i < precision)
    str[i++] = (flags & FLAG_PAD_ZERO) ? '0' : ' ';

  i--;
  while (i >= 0)
    putchar (str[i--]);
}
  

static void
print_signed_nr (int (*putchar) (int),
		 long long nr, int base, int precision, unsigned int flags)
{
  unsigned long long unr;

  if (nr < 0)
    {
      putchar ('-');
      unr = -nr;
    }
  else
    unr = nr;

  print_nr (putchar, unr, base, precision, flags);
}


int
s_cvprintf (int (*putchar) (int), const char *fmt, va_list ap)
{
  int precision = 0;
  unsigned int flags = 0;
  bool done;

  const char *p = fmt;

  while (*p != '\0')
    {
      const char *startp;

      if (*p != '%')
	{
	  putchar (*(p++));
	  continue;
	}
      p++;

      startp = p;
      flags = 0;
      precision = 0;
      done = false;

      while (*p && !done)
	{  
	  switch (*p)
	    {
	    case '%':
	      putchar ('%');
	      p++;
	      done = true;
	      break;

	    case '#':
	      flags |= FLAG_BASE;
	      p++;
	      break;

	    case '0':
	      flags |= FLAG_PAD_ZERO;
	      p++;
	      break;

	    case '1':
	    case '2':
	    case '3':
	    case '4':
	    case '5':
	    case '6':
	    case '7':
	    case '8':
	    case '9':
	      while (*p >= '0' && *p <= '9')
		{
		  precision = precision * 10;
		  precision += *(p++) - '0';
		}
	      break;

	    case 'l':
	      p++;
	      if (*p == 'l')
		{
		  p++;
		  flags |= FLAG_LONGLONG;
		}
	      else
		flags |= FLAG_LONG;
	      break;

	    case 'o':
	      if (flags & FLAG_LONGLONG)
		print_nr (putchar,
			  va_arg (ap, unsigned long long), 8,
			  precision, flags);
	      else if (flags & FLAG_LONG)
		print_nr (putchar,
			  va_arg (ap, unsigned long), 8,
			  precision, flags);
	      else
		print_nr (putchar,
			  va_arg (ap, unsigned int), 8, precision, flags);
	      p++;
	      done = true;
	      break;
	      
	    case 'd':
	    case 'i':
	      if (flags & FLAG_LONGLONG)
		print_signed_nr (putchar,
				 va_arg (ap, long long), 10,
				 precision, flags);
	      else if (flags & FLAG_LONG)
		print_signed_nr (putchar,
				 va_arg (ap, long), 10,
				 precision, flags);
	      else
		print_signed_nr (putchar,
				 va_arg (ap, int), 10, precision, flags);
	      p++;
	      done = true;
	      break;
	      
	    case 'X':
	      flags |= FLAG_UPPERCASE;
	      /* Fall-through.  */
	    case 'x':
	      if (flags & FLAG_LONGLONG)
		print_nr (putchar,
			  va_arg (ap, unsigned long long), 16,
			  precision, flags);
	      else if (flags & FLAG_LONG)
		print_nr (putchar,
			  va_arg (ap, unsigned long), 16,
			  precision, flags);
	      else
		print_nr (putchar,
			  va_arg (ap, unsigned int), 16, precision, flags);
	      p++;
	      done = true;
	      break;
	      
	    case 'u':
	      if (flags & FLAG_LONGLONG)
		print_nr (putchar,
			  va_arg (ap, unsigned long long), 10,
			  precision, flags);
	      else if (flags & FLAG_LONG)
		print_nr (putchar,
			  va_arg (ap, unsigned long), 10,
			  precision, flags);
	      else
		print_nr (putchar,
			  va_arg (ap, unsigned int), 10, precision, flags);
	      p++;
	      done = true;
	      break;
	      
	    case 'c':
	      putchar (va_arg (ap, int));
	      p++;
	      done = true;
	      break;
	      
	    case 's':
	      {
		char *str = va_arg (ap, char *);
		while (*str)
		  putchar (*(str++));
	      }
	      p++;
	      done = true;
	      break;
	      
	    case 'P':
	      flags |= FLAG_UPPERCASE;
	      /* Fall-through.  */
	    case 'p':
	      flags |= FLAG_BASE;
	      print_nr (putchar,
			(uintptr_t) va_arg (ap, void *), 16,
			precision, flags);
	      p++;
	      done = true;
	      break;
	      
	    default:
	      putchar ('%');
	      p++;
	      while (startp < p)
		putchar (*(startp++));
	      done = true;
	      break;
	    }
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
