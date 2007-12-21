/* output.h - Output routines interfaces.
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

#ifndef _OUTPUT_H
#define _OUTPUT_H	1

#include <stdint.h>
#include <stdbool.h>

/* Every architecture must define at least one output driver, but might
   define several.  For each output driver, the name and operations on
   the driver must be provided in the following structure.  */

struct output_driver
{
  const char *name;

  /* Initialize the output device.  */
  void (*init) (struct output_driver *device, const char *cfg);

  /* Deinitialize the output device.  */
  void (*deinit) (struct output_driver *device);

  /* Output a character.  */
  void (*putchar) (struct output_driver *device, int chr);

  /* Read a character.  */
  int (*getchar) (struct output_driver *device);

  uintptr_t cookie;
};


/* Every architecture must provide a list of all output drivers,
   terminated by a driver structure which has a null pointer as its
   name.  */
extern struct output_driver *output_drivers[];


#include <stdarg.h>

/* Activate the output device described by CONF.  CONF has the pattern
   NAME[,CONFIG...], for example "serial,uart2,speed=9600".  If CONF
   is NULL, then the default device is used.  Store the device
   instance in * the storage designated by *DEVICE.  If MAKE_DEFAULT
   is true, on success device configuration, make the device the
   default i/o device.  If there is already a default i/o device, that
   driver is first deinitialized.  Returns false if DRIVER is not a
   valid output driver specification, otherwise true on success.  */
bool output_init (struct output_driver *device, const char *conf,
		  bool make_default);


/* Deactivate the output driver.  Must be called after the last time
   putchar or any other output routine is called.  */
void output_deinit (struct output_driver *device);


/* Print the single character CHR on the output device.  */
int device_putchar (struct output_driver *device, int chr);
int putchar (int chr);

int device_puts (struct output_driver *device, const char *str);
int puts (const char *str);

int device_vprintf (struct output_driver *device, const char *fmt, va_list ap);
int vprintf (const char *fmt, va_list ap);

int device_printf (struct output_driver *device, const char *fmt, ...);
int printf (const char *fmt, ...);

int device_getchar (struct output_driver *device);
int getchar (void);

#endif	/* _OUTPUT_H */
