/* output-serial.c - A serial port output driver.
   Copyright (C) 2003, 2008 Free Software Foundation, Inc.
   Written by Daniel Wagner.

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

#include <sys/io.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>

#include <l4/ipc.h>

#include "output.h"


/* The base I/O ports for the serial device ports COM1 and COM2.  */
#define UART1_BASE	0x3f8
#define UART2_BASE	0x2f8

/* The default base port.  */
#define UART_BASE_DEFAULT UART1_BASE;

/* The data register.  */
#define UART_DR		(device->cookie + 0)

/* The interrupt enable and ID registers.  */
#define UART_IER	(device->cookie + 1)
#define UART_IIR	(device->cookie + 2)

/* The line and modem control and status registers.  */
#define UART_LCR	(device->cookie + 3)
#define UART_MCR	(device->cookie + 4)
#define UART_LSR	(device->cookie + 5)
/* Data ready.  */
#define UART_LSR_DR	(1 << 0)
/* Empty transmitter holding register.  */
#define UART_LSR_THRE	(1 << 5)
#define UART_MSR	(device->cookie + 6)


/* Baudrate divisor LSB and MSB registers.  */
#define UART_LSB (device->cookie + 0)
#define UART_MSB (device->cookie + 1)

/* The default speed setting.  */
#define UART_SPEED_MAX		115200
#define UART_SPEED_MIN		50
#define UART_SPEED_DEFAULT	UART_SPEED_MAX


static void
serial_init (struct output_driver *device, const char *driver_cfg)
{
  /* Twice the desired UART speed, to allow for .5 values.  */
  unsigned int uart_speed = 2 * UART_SPEED_DEFAULT;
  unsigned int divider;

  device->cookie = UART_BASE_DEFAULT;
  if (driver_cfg)
    {      
      char *cfg = strdupa (driver_cfg);

      char *token = cfg;
      bool done = false;
      while (*cfg && *cfg != ',' )
	cfg ++;
      if (*cfg == 0)
	done = true;
      *cfg = 0;

      while (token)
	{
	  if (!strcmp (token, "uart1"))
	    device->cookie = UART1_BASE;
	  if (!strcmp (token, "uart2"))
	    device->cookie = UART2_BASE;
	  if (!strncmp (token, "speed=", 6))
	    {
	      char *tail;
	      unsigned long new_speed;
	      
	      errno = 0;
	      new_speed = strtoul (&token[6], &tail, 0);
	      
	      /* Allow .5 for speeds like 134.5.  */
	      new_speed <<= 1;
	      if (tail[0] == '.' && tail[1] == '5')
		{
		  new_speed++;
		  tail += 2;
		}
	      if (!errno && !*tail
		  && new_speed > UART_SPEED_MIN && new_speed < UART_SPEED_MAX)
		uart_speed = new_speed;
	    }

	  if (done)
	    token = NULL;
	  else
	    {
	      while (*cfg && *cfg != ',' )
		cfg ++;
	      if (*cfg == 0)
		done = true;
	      *cfg = 0;
	    }
	}
    }

  /* Parity bit.  */
  outb (0x80, UART_LCR);

  /* FIXME: How long do we have to wait? */
  l4_sleep (l4_time_period (L4_WORD_C (100000)));

  /* Set baud rate. */
  divider = (2 * UART_SPEED_MAX) / uart_speed;
  outb ((divider >> 0) & 0xff, UART_LSB);
  outb ((divider >> 8) & 0xff, UART_MSB);

  /* Set 8,N,1.  */
  outb (0x03, UART_LCR);

  /* Disable interrupts.  */
  outb (0x00, UART_IER);

  /* Enable FIFOs. */
  outb (0x07, UART_IIR);

  /* Enable RX interrupts.  */
  outb (0x01, UART_IER);

  inb (UART_IER);
  inb (UART_IIR);
  inb (UART_LCR);
  inb (UART_MCR);
  inb (UART_LSR);
  inb (UART_MSR);
}


static void
serial_putchar (struct output_driver *device, int chr)
{
  while (!(inb (UART_LSR) & UART_LSR_THRE))
    ;

  outb (chr, UART_DR);

  if (chr == '\n')
    serial_putchar (device, '\r');
}

static int
serial_getchar (struct output_driver *device)
{
  while (!((inb (UART_LSR)) & UART_LSR_DR))
    ;

  return inb (UART_DR);
}

struct output_driver serial_output =
  {
    "serial",
    serial_init,
    0,			/* deinit */
    serial_putchar,
    serial_getchar
  };
