/* output.c - Output routines.
   Copyright (C) 2003 Free Software Foundation, Inc.
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

#if HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdarg.h>

#include <l4.h>

#include "output.h"


/* True if debugging is enabled.  */
int output_debug;


/* Print the single character CHR on the output device.  */
int
putchar (int chr)
{
  l4_msg_t msg;

  l4_msg_clear (&msg);
  /* FIXME: Hard coded message label.  */
#define WORTEL_MSG_PUTCHAR 1
  l4_set_msg_label (&msg, WORTEL_MSG_PUTCHAR);
  /* FIXME: This should be our cap ID.  */
  l4_msg_append_word (&msg, 0);
  l4_msg_append_word (&msg, (l4_word_t) chr);
  l4_msg_load (&msg);
  /* FIXME: Hard coded thread ID.  */
  l4_send (l4_global_id (l4_thread_user_base () + 2, 1));
  /* FIXME: No error handling.  */

  return 0;
}


int
puts (const char *str)
{
  while (*str != '\0')
    putchar (*(str++));

  putchar ('\n');

  return 0;
}


static void
print_nr (unsigned long long nr, int base)
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
print_signed_nr (long long nr, int base)
{
  unsigned long long unr;

  if (nr < 0)
    {
      putchar ('-');
      unr = -nr;
    }
  else
    unr = nr;

  print_nr (unr, base);
}
  

int
printf (const char *fmt, ...)
{
  va_list ap;

  va_start (ap, fmt);
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
	      print_nr (va_arg (ap, unsigned long long), 8);
	      p++;
	      break;

	    case 'd':
	    case 'i':
	      print_signed_nr (va_arg (ap, long long), 10);
	      p++;
	      break;

	    case 'x':
	    case 'X':
	      print_nr (va_arg (ap, unsigned long long), 16);
	      p++;
	      break;

	    case 'u':
	      print_nr (va_arg (ap, unsigned long long), 10);
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
	  print_nr (va_arg (ap, unsigned int), 8);
	  p++;
	  break;

	case 'd':
	case 'i':
	  print_signed_nr (va_arg (ap, int), 10);
	  p++;
	  break;

	case 'x':
	case 'X':
	  print_nr (va_arg (ap, unsigned int), 16);
	  p++;
	  break;

	case 'u':
	  print_nr (va_arg (ap, unsigned int), 10);
	  p++;
	  break;

	case 'c':
	  putchar (va_arg (ap, int));
	  p++;
	  break;

	case 's':
	  {
	    char *str = va_arg (ap, char *);
	    while (*str)
	      putchar (*(str++));
	  }
	  p++;
	  break;

	case 'p':
	  print_nr ((unsigned int) va_arg (ap, void *), 16);
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
