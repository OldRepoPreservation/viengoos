/* deva-class.c - Deva class for the deva server.
   Copyright (C) 2003, 2004 Free Software Foundation, Inc.
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

#include <stdlib.h>

#include <l4.h>
#include <hurd/cap-server.h>
#include <hurd/wortel.h>

#include "deva.h"

/* Do not even think about improving the console!  The code here is
   only for testing.  Go do something useful.  Work on the real device
   driver framework.  */
#define SIMPLE_CONSOLE 1

#ifdef SIMPLE_CONSOLE

#include <sys/io.h>

pthread_mutex_t console_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t console_cond = PTHREAD_COND_INITIALIZER;
#define CONSOLE_MAX 256
unsigned char console_input[CONSOLE_MAX];
unsigned int console_input_len;

static void *
console_irq_handler (void *unused)
{
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

      pthread_mutex_lock (&console_lock);
      if (b == '\x1b')
	asm volatile ("int $3");
      else if (b != '\x00')
	{
	  if (console_input_len == CONSOLE_MAX)
	    {
	      putchar ('^');
	      putchar ('G');
	    }
	  else
	    {
	      putchar (b);
	      if (!console_input_len)
		pthread_cond_broadcast (&console_cond);
	      console_input[console_input_len++] = b;
	    }
	}
      pthread_mutex_unlock (&console_lock);

      l4_load_mr (0, 0);
      l4_reply_receive (irq);
    }
}


static void
console_init (void)
{
  error_t err;
  l4_thread_id_t irq_handler_tid;
  pthread_t irq_handler;

  irq_handler_tid = pthread_pool_get_np ();
  if (irq_handler_tid == l4_nilthread)
    panic ("Can not create the irq handler thread");
  
  err = pthread_create_from_l4_tid_np (&irq_handler, NULL,
				       irq_handler_tid, console_irq_handler,
				       NULL);
  if (err)
    panic ("Can not create the irq handler thread: %i", err);

  pthread_detach (irq_handler);
}

#endif /* SIMPLE_CONSOLE */


struct deva
{
  /* FIXME: More stuff.  */
  int foo;
};
typedef struct deva *deva_t;


static void
deva_reinit (hurd_cap_class_t cap_class, hurd_cap_obj_t obj)
{
  deva_t deva = hurd_cap_obj_to_user (deva_t, obj);

  /* FIXME: Release resources.  */
}


error_t
deva_io_read (hurd_cap_rpc_context_t ctx)
{
#ifdef SIMPLE_CONSOLE
  unsigned char b = '\x00';
  
  pthread_mutex_lock (&console_lock);
  while (!console_input_len)
    pthread_cond_wait (&console_cond, &console_lock);
  b = console_input[--console_input_len];
  pthread_mutex_unlock (&console_lock);

  /* Prepare reply message.  */
  l4_msg_clear (ctx->msg);
  l4_msg_append_word (ctx->msg, (l4_word_t) b);

#endif
  return 0;
}


error_t
deva_io_write (hurd_cap_rpc_context_t ctx)
{
#ifdef SIMPLE_CONSOLE
  l4_word_t chr;

  chr = l4_msg_word (ctx->msg, 1);
  putchar ((unsigned char) chr);
#endif

  return 0;
}


error_t
deva_demuxer (hurd_cap_rpc_context_t ctx)
{
  error_t err = 0;

  switch (l4_msg_label (ctx->msg))
    {
      /* DEVA_IO_READ */
    case 768:
      err = deva_io_read (ctx);
      break;

      /* DEVA_IO_WRITE */
    case 769:
      err = deva_io_write (ctx);
      break;

    default:
      err = EOPNOTSUPP;
    }

  return err;
}



static struct hurd_cap_class deva_class;

/* Initialize the deva class subsystem.  */
error_t
deva_class_init ()
{
#ifdef SIMPLE_CONSOLE
  console_init ();
#endif

  return hurd_cap_class_init (&deva_class, deva_t,
			      NULL, NULL, deva_reinit, NULL,
			      deva_demuxer);
}


/* Allocate a new deva object.  The object returned is locked and has
   one reference.  */
error_t
deva_alloc (hurd_cap_obj_t *r_obj)
{
  error_t err;
  hurd_cap_obj_t obj;
  deva_t deva;

  err = hurd_cap_class_alloc (&deva_class, &obj);
  if (err)
    return err;

  deva = hurd_cap_obj_to_user (deva_t, obj);

  /* FIXME: Add some stuff.  */

  *r_obj = obj;
  return 0;
}
