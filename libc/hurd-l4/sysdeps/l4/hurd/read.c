/* Copyright (C) 1991,1995,1996,1997, 2002, 2005 Free Software Foundation, Inc.
   This file is part of the GNU C Library.

   The GNU C Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.

   The GNU C Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with the GNU C Library; if not, write to the Free
   Software Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
   02111-1307 USA.  */

#include <errno.h>
#include <unistd.h>
#include <stddef.h>


#include <stdarg.h>

#include <stdbool.h>

#include <l4/types.h>
#include <l4/space.h>
#include <l4/ipc.h>

#include <hurd/types.h>
#include <hurd/startup.h>

/* Echo the character CHR on the manager console.  */
static inline int
__attribute__((always_inline))
deva_getchar (void)
{
  l4_word_t chr;
  l4_msg_tag_t tag;
  extern struct hurd_startup_data *_hurd_startup_data;
  hurd_cap_handle_t deva_cap_handle
    = _hurd_startup_data->deva_console.cap_handle;
  l4_thread_id_t deva_thread_id = _hurd_startup_data->deva_console.server;

  //  l4_accept (L4_UNTYPED_WORDS_ACCEPTOR);

  tag = l4_niltag;
  l4_msg_tag_set_label (&tag, 768 /* DEVA_IO_READ */);
  l4_msg_tag_set_untyped_words (&tag, 1);
  l4_set_msg_tag (tag);
  l4_load_mr (1, (l4_word_t) deva_cap_handle);
  tag = l4_call (deva_thread_id);

  l4_store_mr (1, &chr);
  return (int) chr;
}


/* Read NBYTES into BUF from FD.  Return the number read or -1.  */
ssize_t
__libc_read (int fd, void *buf, size_t nbytes)
{
  size_t res;
  char *buffer = (char *) buf;

  if (nbytes == 0)
    return 0;
  if (fd < 0)
    {
      __set_errno (EBADF);
      return -1;
    }
  if (buf == NULL)
    {
      __set_errno (EINVAL);
      return -1;
    }

  /* FIXME: Only stdin is supported for now.  */
  if (fd != 0)
    {
      __set_errno (EBADF);
      return -1;
    }

  res = 0;
  while (res < nbytes)
    {
      int chr = deva_getchar ();

      buffer[res] = (char) chr;
      res++;
      if (chr == '\n')
	break;
    }

  return res;
}
libc_hidden_def (__libc_read)
stub_warning (read)

weak_alias (__libc_read, __read)
libc_hidden_weak (__read)
weak_alias (__libc_read, read)
#include <stub-tag.h>
