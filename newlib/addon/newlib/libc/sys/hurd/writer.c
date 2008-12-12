#include <reent.h>
#include <unistd.h>
#include <errno.h>

#include "fd.h"

_ssize_t
write (int fdi, const void *buf, size_t cnt)
{
  if (fdi < 0 || fdi >= _fd_size)
    {
      errno = EBADF;
      return -1;
    }

  ss_mutex_lock (&_fd_lock);

  struct _fd *fd = _fds[fdi];
  if (! fd || ! fd->ops->pwrite)
    {
      errno = EBADF;
      ss_mutex_unlock (&_fd_lock);
      return -1;
    }

  if (cnt == 0)
    {
      ss_mutex_unlock (&_fd_lock);
      return 0;
    }

  ss_mutex_lock (&fd->lock);
  ss_mutex_unlock (&_fd_lock);

  _ssize_t len = fd->ops->pwrite (fd, buf, cnt, fd->pos);
  fd->pos += len;

  ss_mutex_unlock (&fd->lock);

  return len;
}


_ssize_t
_DEFUN (_write_r, (ptr, fd, buf, cnt),
     struct _reent *ptr _AND
     int fd _AND
     _CONST _PTR buf _AND
     size_t cnt)
{
  return write (fd, buf, cnt);
}
