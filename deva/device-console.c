/* device-console.c - A small console device, until we have the real thing.
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

#include <l4.h>

#include <hurd/wortel.h>

#include "output.h"

#include "device.h"


/* Do not even think about improving the console!  The code here is
   only for testing.  Go do something useful.  Work on the real device
   driver framework.  */

static void *
console_irq_handler (void *_console)
{
  device_t *dev = (device_t *) _console;
  l4_word_t result;
  l4_thread_id_t irq;
  l4_msg_tag_t tag;
      
#define KEYBOARD_IRQ	1
#define KEYBOARD_INPUT	0x60

  irq = l4_global_id (KEYBOARD_IRQ, 1);
  result = wortel_thread_control (irq, irq, l4_nilthread, l4_myself (),
				  (void *) -1);
  if (result)
    panic ("setting irq pager failed: %i", result);
 
#define l4_reply_receive(tid)						\
  ({ l4_thread_id_t dummy;						\
  l4_ipc (tid, tid, l4_timeouts (L4_ZERO_TIME, L4_NEVER), &dummy); })
   
  tag = l4_receive (irq);

  while (1)
    {
      unsigned char b;
      
      b = inb (0x60);
      {
	/* This is a conversion table from i8042 scancode set 1 to set 2.  */
	static char sc_map[] = "\x00" "\x1b" "1234567890-=" "\x7f"
	  "\x09" "qwertyuiop[]" "\x0a"
	  "\x00" /* left ctrl */ "asdfghjkl;'`"
	  "\x00" /* left shift */ "\\zxcvbnm,./" "\x00" /* right shift */
	  "*" "\x00" /* left alt */ " ";

	if (b < sizeof (sc_map))
	  b = sc_map[b];
	else
	  b = '\x00';
      }

      pthread_mutex_lock (&dev->console.lock);
      if (b == '\x1b')
	asm volatile ("int $3");
      else if (b != '\x00')
	{
	  if (dev->console.input_len == CONSOLE_MAX)
	    {
	      putchar ('^');
	      putchar ('G');
	    }
	  else
	    {
	      putchar (b);
	      if (!dev->console.input_len)
		pthread_cond_broadcast (&dev->console.cond);
	      dev->console.input[dev->console.input_len++] = b;
	    }
	}
      pthread_mutex_unlock (&dev->console.lock);

      l4_load_mr (0, 0);
      l4_reply_receive (irq);
    }
}


static void
console_init (device_t *dev)
{
  error_t err;
  l4_thread_id_t irq_handler_tid;
  pthread_t irq_handler;
  l4_word_t result;

  irq_handler_tid = pthread_pool_get_np ();
  if (irq_handler_tid == l4_nilthread)
    panic ("Can not create the kbd irq handler thread");

  /* FIXME: We just tweak the scheduler so we can set the priority
     ourselves.  */
  result =  wortel_thread_control (irq_handler_tid, irq_handler_tid,
				   l4_myself (), l4_nilthread, (void *) -1);
  if (result)
    panic ("Can not set scheduler for kbd irq handler thread: %i", result);
  result = l4_set_priority (irq_handler_tid, /* FIXME */ 150);
  if (!result)
    panic ("Can not set priority for kbd irq handler thread: %i", result);

  err = pthread_create_from_l4_tid_np (&irq_handler, NULL,
				       irq_handler_tid, console_irq_handler,
				       dev);
  if (err)
    panic ("Can not create the kbd irq handler thread: %i", err);

  pthread_detach (irq_handler);
}


static error_t
console_io_read (device_t *dev, int *chr)
{
  unsigned char b = '\x00';

  pthread_mutex_lock (&dev->console.lock);
  while (!dev->console.input_len)
    pthread_cond_wait (&dev->console.cond, &dev->console.lock);
  b = dev->console.input[0];
  dev->console.input_len--;
  memmove (&dev->console.input[0], &dev->console.input[1],
	   dev->console.input_len);
  pthread_mutex_unlock (&dev->console.lock);

  *chr = b;

  return 0;
}


static error_t
console_io_write (device_t *dev, int chr)
{
  putchar ((unsigned char) chr);

  return 0;
}


/* Create a new console device in DEV.  */
error_t
device_console_init (device_t *dev)
{
  dev->io_read = console_io_read;
  dev->io_write = console_io_write;
  dev->console.lock = (pthread_mutex_t) PTHREAD_MUTEX_INITIALIZER;
  dev->console.cond = (pthread_cond_t) PTHREAD_COND_INITIALIZER;
  dev->console.input_len = 0;

  /* We know this is only called once, so what the hell.  */
  console_init (dev);

  return 0;
}
