/* device-serial.c - A small serial device, until we have the real thing.
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

#if HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>
#include <pthread.h>
#include <sys/io.h>

#include <hurd/wortel.h>
#include "device.h"
#include "output.h"


/* Do not even think about improving this driver!  The code here is
   only for testing.  Go do something useful.  Work on the real device
   driver framework.  */


/* The base I/O ports for the serial device ports COM1 and COM2.  */
#define UART1_BASE	0x3f8
#define UART1_IRQ	4
#define UART2_BASE	0x2f8
#define UART2_IRQ	3

/* The selected base port.  */
static unsigned short int uart_base = UART1_BASE;
static unsigned short int uart_irq = UART1_IRQ;

/* The data register.  */
#define UART_DR		(uart_base + 0)

/* The interrupt enable and ID registers.  */
#define UART_IER	(uart_base + 1)
#define UART_IIR	(uart_base + 2)

/* The line and modem control and status registers.  */
#define UART_LCR	(uart_base + 3)
#define UART_MCR	(uart_base + 4)
#define UART_LSR	(uart_base + 5)
#define UART_LSR_THRE	(1 << 5)
#define UART_MSR	(uart_base + 6)


/* Baudrate divisor LSB and MSB registers.  */
#define UART_LSB (uart_base + 0)
#define UART_MSB (uart_base + 1)

/* The default speed setting.  */
#define UART_SPEED_MAX		115200
#define UART_SPEED_MIN		50
#define UART_SPEED_DEFAULT	UART_SPEED_MAX


static void *
serial_irq_handler (void *_serial)
{
  device_t *dev = (device_t *) _serial;
  l4_word_t result;
  l4_thread_id_t irq;
  l4_msg_tag_t tag;
  unsigned int uart_speed = 2 * UART_SPEED_DEFAULT;
  unsigned int divider;

  /* Disable interrupts.  */
  outb (0x00, UART_IER);

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
      
  irq = l4_global_id (uart_irq, 1);
  result = wortel_thread_control (irq, irq, l4_nilthread, l4_myself (),
				  (void *) -1);
  if (result)
    panic ("setting serial irq pager failed: %i", result);
 
#define l4_reply_receive(tid)						\
  ({ l4_thread_id_t dummy;						\
  l4_ipc (tid, tid, l4_timeouts (L4_ZERO_TIME, L4_NEVER), &dummy); })
   
  tag = l4_receive (irq);

  while (1)
    {
      unsigned char b;
      
      /* Check if the interrupt really came from the UART.  */
      b = inb (UART_IIR);
      if (b & 1)
	/* No interrupt pending.  */
	debug ("Spurious interrupt irq=%i", uart_irq);
      else
	{
	  pthread_mutex_lock (&dev->serial.lock);
	  
	  b = inb (UART_LSR);
	  if (b & 1)
	    {
	      /* receive data ready */
	      
	      do
		{
		  unsigned char c = inb (UART_DR);

		  if (dev->serial.input_len == 0 && dev->serial.input_wait)
		    {
		      /* We have some new input.  */
		      pthread_cond_broadcast (&dev->serial.cond);
		      dev->serial.input_wait = 0;
		    }
		      
		  if (dev->serial.input_len < SERIAL_MAX)
		    {
		      /* If we get too much data, we just drop some.  */
		      dev->serial.input[dev->serial.input_len++] = c;
		    }
debug("%c", c);		  
		  /* Update the status.  */
		  b = inb (UART_LSR);
		}
	      while (b & 1);
	    }
	  
	  {
	    int len = dev->serial.output_len;
	    int tx = 0;

	    while (tx < len && (b & (1 << 5)))
	      {
		/* transmit hold ready (empty).  */

		unsigned char c = dev->serial.output[tx++];

		outb (c, UART_DR);

debug("[%c]", c);		
		/* Update the status.  */
		b = inb (UART_LSR);
	      }

	    if (tx != 0)
	      {
		if (len == SERIAL_MAX && dev->serial.output_wait)
		  {
		    /* There is now some more space.  */
		    pthread_cond_broadcast (&dev->serial.cond);
		    dev->serial.output_wait = 0;
		  }

		len -= tx;
		dev->serial.output_len = len;

		if (len == 0)
		  /* Disable interrupts for TX.  */
		  outb (UART_IER, 0x1);
		    
		else
		  memmove (&dev->serial.output[0],
			   &dev->serial.output[tx], len);
	      }
	    }
	  pthread_mutex_unlock (&dev->serial.lock);
	}	  

      l4_load_mr (0, 0);
      l4_reply_receive (irq);
    }
}


static void
serial_init (device_t *dev)
{
  error_t err;
  l4_thread_id_t irq_handler_tid;
  pthread_t irq_handler;
  l4_word_t result;

  irq_handler_tid = pthread_pool_get_np ();
  if (irq_handler_tid == l4_nilthread)
    panic ("Can not create the irq handler thread");
  
  /* FIXME: We just tweak the scheduler so we can set the priority
     ourselves.  */
  result =  wortel_thread_control (irq_handler_tid, irq_handler_tid,
				   l4_myself (), l4_nilthread, (void *) -1);
  if (result)
    panic ("Can not set scheduler for serial irq handler thread: %i", result);
  result = l4_set_priority (irq_handler_tid, /* FIXME */ 150);
  if (!result)
    panic ("Can not set priority for serial irq handler thread: %i", result);

  err = pthread_create_from_l4_tid_np (&irq_handler, NULL,
				       irq_handler_tid, serial_irq_handler,
				       dev);
  if (err)
    panic ("Can not create the irq handler thread: %i", err);

  pthread_detach (irq_handler);
}


static error_t
serial_io_read (device_t *dev, int *chr)
{
  pthread_mutex_lock (&dev->serial.lock);
  while (!dev->serial.input_len)
    {
      dev->serial.input_wait = 1;
      pthread_cond_wait (&dev->serial.cond, &dev->serial.lock);
    }
  *chr = dev->serial.input[0];
  dev->serial.input_len--;
  memmove (&dev->serial.input[0], &dev->serial.input[1],
	   dev->serial.input_len);
  pthread_mutex_unlock (&dev->serial.lock);

  return 0;
}


static error_t
serial_io_write (device_t *dev, int chr)
{
  int old_len;

  pthread_mutex_lock (&dev->serial.lock);
  while (dev->serial.output_len == SERIAL_MAX)
    {
      dev->serial.output_wait = 1;
      pthread_cond_wait (&dev->serial.cond, &dev->serial.lock);
    }
  old_len = dev->serial.output_len;
  dev->serial.output[++dev->serial.output_len] = chr;

  if (old_len == 0)
    {
      /* Enable interrupts for TX.  */
      outb (UART_IER, 0x3);
    }
  pthread_mutex_unlock (&dev->serial.lock);

  return 0;
}


/* Create a new console device in DEV.  */
error_t
device_serial_init (device_t *dev)
{
  dev->io_read = serial_io_read;
  dev->io_write = serial_io_write;
  dev->serial.lock = (pthread_mutex_t) PTHREAD_MUTEX_INITIALIZER;
  dev->serial.cond = (pthread_cond_t) PTHREAD_COND_INITIALIZER;
  dev->serial.input_len = 0;
  dev->serial.input_wait = 0;
  dev->serial.output_len = 0;
  dev->serial.output_wait = 0;

  /* We know this is only called once, so what the hell.  */
  serial_init (dev);

  return 0;
}
