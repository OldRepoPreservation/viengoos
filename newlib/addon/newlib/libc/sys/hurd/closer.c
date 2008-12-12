/* Reentrant version of close system call.  */

#include <reent.h>
#include <errno.h>
#include "fd.h"

int
close (int fdi)
{
  if (fdi < 0 || fdi >= _fd_size)
    {
      errno = EBADF;
      return -1;
    }

  ss_mutex_lock (&_fd_lock);

  struct _fd *fd = _fds[fdi];
  if (! fd)
    {
      errno = EBADF;
      ss_mutex_unlock (&_fd_lock);
      return -1;
    }

  ss_mutex_lock (&fd->lock);
  ss_mutex_unlock (&_fd_lock);

  fd->ops->close (fd);
  _fds[fdi] = NULL;

  ss_mutex_unlock (&fd->lock);

  return 0;
}

int
_close_r (ptr, fd)
     struct _reent *ptr;
     int fd;
{
  return close (fd);
}
