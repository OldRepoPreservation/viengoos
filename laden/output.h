/* output.h - Output routines interfaces.
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

#ifndef _OUTPUT_H
#define _OUTPUT_H	1


/* Every architecture must define at least one output driver, but might
   define several.  For each output driver, the name and operations on
   the driver must be provided in the following structure.  */

struct output_driver
{
  const char *name;

  /* Initialize the output device.  */
  void (*init) (const char *cfg);

  /* Deinitialize the output device.  */
  void (*deinit) (void);

  /* Output a character.  */
  void (*putchar) (int chr);
};


/* Every architecture must provide a list of all output drivers,
   terminated by a driver structure which has a null pointer as its
   name.  */
extern struct output_driver *output_drivers[];


/* Activate the output driver DRIVER or the default one if DRIVER is a
   null pointer.  Must be called once at startup, before calling
   putchar or any other output routine.  DRIVER has the pattern
   NAME[,CONFIG...], for example "serial,uart2,speed=9600".  Returns 0
   if DRIVER is not a valid output driver specification, otherwise 1
   on success.  */
int output_init (const char *driver);


/* Deactivate the output driver.  Must be called after the last time
   putchar or any other output routine is called.  */
void output_deinit (void);


/* Print the single character CHR on the output device.  */
int putchar (int chr);

int puts (const char *str);

int printf (const char *fmt, ...);

/* True if debug mode is enabled.  */
extern int output_debug;


/* Print a debug message.  */
#define debug(...) do { if (output_debug) printf (__VA_ARGS__); } while (0)

#endif	/* _OUTPUT_H */
