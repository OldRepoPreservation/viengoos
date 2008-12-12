#include <unistd.h>
#include <errno.h>

#include "fd.h"

int
fsync (int fdi)
{
  if (fdi < 0 || fdi >= _fd_size)
    {
      errno = EBADF;
      return -1;
    }

  ss_mutex_lock (&_fd_lock);

  struct _fd *fd = _fds[fdi];
  ss_mutex_unlock (&_fd_lock);
  if (! fd)
    {
      errno = EBADF;
      return -1;
    }

  return 0;
}
