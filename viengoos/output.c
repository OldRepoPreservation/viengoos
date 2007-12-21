/* output.c - Output routines.
   Copyright (C) 2003, 2007 Free Software Foundation, Inc.
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

#include <hurd/stddef.h>
#include <stdarg.h>
#include <string.h>

#include "output.h"

/* The active output driver.  */
static struct output_driver *default_device;


/* Activate the output driver NAME or the default one if NAME is a
   null pointer.  Must be called once at startup, before calling
   putchar or any other output routine.  Returns 0 if NAME is not a
   valid output driver name, otherwise 1 on success.  */
bool
output_init (struct output_driver *device, const char *conf,
	     bool make_default)
{
  const char *driver_args = NULL;

  struct output_driver **out = &output_drivers[0];
  if (conf)
    {
      while (*out)
	{
	  unsigned int name_len = strlen ((*out)->name);
	  if (!strncmp (conf, (*out)->name, name_len))
	    {
	      const char *cfg = conf + name_len;
	      if (!*cfg || *cfg == ',') 
		{
		  if (*cfg)
		    driver_args = cfg + 1;
		  break;
		}
	    }
	  out++;
	}
      if (!*out)
	return false;
    }

  if (make_default && default_device)
    {
      output_deinit (default_device);
      default_device = 0;
    }

  *device = **out;

  if (device->init)
    (*device->init) (device, driver_args);

  if (make_default)
    default_device = device;

  return true;
}


/* Deactivate the output driver.  Must be called after the last time
   putchar or any other output routine is called, and before control
   is passed on to the L4 kernel.  */
void
output_deinit (struct output_driver *device)
{
  if (device && device->deinit)
    (*device->deinit) (device);
}


/* Print the single character CHR on the output device.  */
int
device_putchar (struct output_driver *device, int chr)
{
  if (device && device->putchar)
    device->putchar (device, chr);
  return 0;
}

int
putchar (int chr)
{
  return device_putchar (default_device, chr);
}


int
device_puts (struct output_driver *device, const char *str)
{
  while (*str != '\0')
    device_putchar (device, *(str++));

  device_putchar (device, '\n');

  return 0;
}

int
puts (const char *str)
{
  return device_puts (default_device, str);
}


static void
print_nr (struct output_driver *device, unsigned long long nr, int base)
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
    device_putchar (device, str[i--]);
}
  

static void
print_signed_nr (struct output_driver *device, long long nr, int base)
{
  unsigned long long unr;

  if (nr < 0)
    {
      device_putchar (device, '-');
      unr = -nr;
    }
  else
    unr = nr;

  print_nr (device, unr, base);
}
  

int
device_vprintf (struct output_driver *device, const char *fmt, va_list ap)
{
  const char *p = fmt;

  while (*p != '\0')
    {
      if (*p != '%')
	{
	  device_putchar (device, *(p++));
	  continue;
	}

      p++;
      switch (*p)
	{
	case '%':
	  device_putchar (device, '%');
	  p++;
	  break;

	case 'l':
	  p++;
	  if (*p != 'l')
	    {
	      device_putchar (device, '%');
	      device_putchar (device, 'l');
	      device_putchar (device, *(p++));
	      continue;
	    }
	  p++;
	  switch (*p)
	    {
	    case 'o':
	      print_nr (device, va_arg (ap, unsigned long long), 8);
	      p++;
	      break;

	    case 'd':
	    case 'i':
	      print_signed_nr (device, va_arg (ap, long long), 10);
	      p++;
	      break;

	    case 'x':
	    case 'X':
	      print_nr (device, va_arg (ap, unsigned long long), 16);
	      p++;
	      break;

	    case 'u':
	      print_nr (device, va_arg (ap, unsigned long long), 10);
	      p++;
	      break;

	    default:
	      device_putchar (device, '%');
	      device_putchar (device, 'l');
	      device_putchar (device, 'l');
	      device_putchar (device, *(p++));
	      break;
	    }
	  break;

	case 'o':
	  print_nr (device, va_arg (ap, unsigned int), 8);
	  p++;
	  break;

	case 'd':
	case 'i':
	  print_signed_nr (device, va_arg (ap, int), 10);
	  p++;
	  break;

	case 'x':
	case 'X':
	  print_nr (device, va_arg (ap, unsigned int), 16);
	  p++;
	  break;

	case 'u':
	  print_nr (device, va_arg (ap, unsigned int), 10);
	  p++;
	  break;

	case 'c':
	  device_putchar (device, va_arg (ap, int));
	  p++;
	  break;

	case 's':
	  {
	    char *str = va_arg (ap, char *);
	    while (*str)
	      device_putchar (device, *(str++));
	  }
	  p++;
	  break;

	case 'p':
	  device_putchar (device, '0');
	  device_putchar (device, 'x');
	  print_nr (device, (unsigned int) va_arg (ap, void *), 16);
	  p++;
	  break;

	default:
	  device_putchar (device, '%');
	  device_putchar (device, *p);
	  p++;
	  break;
	}
    }

  return 0;
}

int
vprintf (const char *fmt, va_list ap)
{
  return device_vprintf (default_device, fmt, ap);
}


int
device_printf (struct output_driver *device, const char *fmt, ...)
{
  va_list ap;

  va_start (ap, fmt);
  int r = device_vprintf (device, fmt, ap);
  va_end (ap);

  return r;
}

int
printf (const char *fmt, ...)
{
  va_list ap;

  va_start (ap, fmt);
  int r = vprintf (fmt, ap);
  va_end (ap);

  return r;
}

int
device_getchar (struct output_driver *device)
{
  if (! device)
    panic ("Attempt to read but no device given!");

  if (! device->getchar)
    panic ("Attempt to read from device (%s) with no input stream",
	   device->name);

  return device->getchar (device);
}

int
getchar (void)
{
  if (! default_device)
    panic ("Attempt to read but no default device configured");

  return device_getchar (default_device);
}

