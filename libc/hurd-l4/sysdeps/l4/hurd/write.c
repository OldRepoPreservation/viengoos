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

#include <sysdep.h>
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
static inline void
__attribute__((always_inline))
deva_putchar (int chr)
{
  l4_msg_tag_t tag;
  extern struct hurd_startup_data *_hurd_startup_data;
  hurd_cap_handle_t deva_cap_handle
    = _hurd_startup_data->deva_console.cap_handle;
  l4_thread_id_t deva_thread_id = _hurd_startup_data->deva_console.server;

  //  l4_accept (L4_UNTYPED_WORDS_ACCEPTOR);

  tag = l4_niltag;
  l4_msg_tag_set_label (&tag, 769 /* DEVA_IO_WRITE */);
  l4_msg_tag_set_untyped_words (&tag, 2);
  l4_set_msg_tag (tag);
  l4_load_mr (1, (l4_word_t) deva_cap_handle);
  l4_load_mr (2, (l4_word_t) chr);
  tag = l4_call (deva_thread_id);
}


/* Write NBYTES of BUF to FD.  Return the number written, or -1.  */
ssize_t
__libc_write (int fd, const void *buf, size_t nbytes)
{
  int res;

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

  /* FIXME: Only stdout/stderr is supported for now.  */
  if (fd != 1 && fd != 2)
    {
      __set_errno (EBADF);
      return -1;
    }

  res = nbytes;
  while (nbytes--)
    deva_putchar (*(((char *)buf)++));

  return res;
}
libc_hidden_def (__libc_write)
stub_warning (write)

weak_alias (__libc_write, __write)
libc_hidden_weak (__write)
weak_alias (__libc_write, write)
#include <stub-tag.h>
