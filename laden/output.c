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

#include <stdlib.h>
#include <stdarg.h>
#include <string.h>

#include "output.h"


/* True if debugging is enabled.  */
int output_debug;


/* The active output driver.  */
static struct output_driver *output;


/* Activate the output driver NAME or the default one if NAME is a
   null pointer.  Must be called once at startup, before calling
   putchar or any other output routine.  Returns 0 if NAME is not a
   valid output driver name, otherwise 1 on success.  */
int
output_init (const char *driver)
{
  const char *driver_cfg = NULL;

  if (output)
    {
      output_deinit ();
      output = 0;
    }

  if (driver)
    {
      struct output_driver **out = &output_drivers[0];
      while (*out)
	{
	  unsigned int name_len = strlen ((*out)->name);
	  if (!strncmp (driver, (*out)->name, name_len))
	    {
	      const char *cfg = driver + name_len;
	      if (!*cfg || *cfg == ',') 
		{
		  if (*cfg)
		    driver_cfg = cfg + 1;
		  output = *out;
		  break;
		}
	    }
	  out++;
	}
      if (!output)
	return 0;
    }
  else
    output = output_drivers[0];

  if (output->init)
    (*output->init) (driver_cfg);

  return 1;
}


/* Deactivate the output driver.  Must be called after the last time
   putchar or any other output routine is called, and before control
   is passed on to the L4 kernel.  */
void
output_deinit (void)
{
  if (output && output->deinit)
    (*output->deinit) ();
}


/* Print the single character CHR on the output device.  */
int
putchar (int chr)
{
  if (!output)
    output_init (0);

  if (output->putchar)
    (*output->putchar) (chr);

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
	  putchar ('0');
	  putchar ('x');
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
