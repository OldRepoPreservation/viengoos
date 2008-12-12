#include <unistd.h>
#include <errno.h>

#include "fd.h"

int
ftruncate (int fdi, off_t length)
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

  int ret;
  if (fd->ops->ftruncate)
    ret = fd->ops->ftruncate (fd, length);
  else
    {
      ret = -1;
      errno = EPIPE;
    }

  ss_mutex_unlock (&fd->lock);

  return ret;
}
