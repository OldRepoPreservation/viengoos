/* device.h - A small device interface, until we have the real thing.
   Copyright (C) 2005 Free Software Foundation, Inc.
   Written by Marcus Brinkmann.

   This file is part of the GNU Hurd.

   The GNU Hurd is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.

   The GNU Hurd is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with the GNU C Library; if not, write to the Free
   Software Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
   02111-1307 USA.  */

#ifndef DEVICE_H
#define DEVICE_H 1

#include <errno.h>


typedef struct device device_t;

struct device
{
  /* Read a single character from DEV and return it in CHR.  */
  error_t (*io_read) (device_t *dev, int *chr);

  /* Write the single character CHR to DEV.  */
  error_t (*io_write) (device_t *dev, int chr);

  /* Private data area.  */
  union
  {
    /* The data used by the console driver.  */
    struct console_data
    {
      pthread_mutex_t lock;
      pthread_cond_t cond;
#define CONSOLE_MAX 256
      unsigned char input[CONSOLE_MAX];
      unsigned int input_len;
    } console;

    /* The data used by the serial driver.  */
    struct serial_data
    {
      pthread_mutex_t lock;
      pthread_cond_t cond;
#define SERIAL_MAX 128
      unsigned char input[SERIAL_MAX];
      unsigned int input_len;
      unsigned int input_wait;
      unsigned char output[SERIAL_MAX];
      unsigned int output_len;
      unsigned int output_wait;
    } serial;
  };
};
;


/* Create a new console device in DEV.  */
error_t device_console_init (device_t *dev);

/* Create a new serial device in DEV.  */
error_t device_serial_init (device_t *dev);

#endif	/* DEVICE_H */
