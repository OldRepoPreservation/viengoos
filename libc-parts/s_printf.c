/* s_printf.c - Simply output routines.
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
  rm_putchar (chr);
  return 0;
#endif
}

int
s_puts (const char *str)
{
  while (*str != '\0')
    s_putchar (*(str++));

  s_putchar ('\n');

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
    s_putchar (str[i--]);
}
  

static void
print_signed_nr (long long nr, int base)
{
  unsigned long long unr;

  if (nr < 0)
    {
      s_putchar ('-');
      unr = -nr;
    }
  else
    unr = nr;

  print_nr (unr, base);
}
  

int
s_vprintf (const char *fmt, va_list ap)
{
  const char *p = fmt;

  while (*p != '\0')
    {
      if (*p != '%')
	{
	  s_putchar (*(p++));
	  continue;
	}

      p++;
      switch (*p)
	{
	case '%':
	  s_putchar ('%');
	  p++;
	  break;

	case 'l':
	  p++;
	  if (*p != 'l')
	    {
	      s_putchar ('%');
	      s_putchar ('l');
	      s_putchar (*(p++));
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
	      s_putchar ('%');
	      s_putchar ('l');
	      s_putchar ('l');
	      s_putchar (*(p++));
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
	  s_putchar (va_arg (ap, int));
	  p++;
	  break;

	case 's':
	  {
	    char *str = va_arg (ap, char *);
	    if (str)
	      while (*str)
		s_putchar (*(str++));
	    else
	      {
		s_putchar ('N');
		s_putchar ('U');
		s_putchar ('L');
		s_putchar ('L');
	      }
	  }
	  p++;
	  break;

	case 'p':
	  print_nr ((unsigned int) va_arg (ap, void *), 16);
	  p++;
	  break;

	default:
	  s_putchar ('%');
	  s_putchar (*p);
	  p++;
	  break;
	}
    }

  return 0;
}

int
s_printf (const char *fmt, ...)
{
  va_list ap;

  va_start (ap, fmt);
  int r = s_vprintf (fmt, ap);
  va_end (ap);
  return r;
}
