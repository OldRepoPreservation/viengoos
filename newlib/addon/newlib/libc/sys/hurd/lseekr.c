#include <reent.h>
#include <unistd.h>
#include <errno.h>

#include "fd.h"

_off_t
lseek (int fdi, _off_t pos, int whence)
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

  if (fd->size == -1)
    {
      errno = EPIPE;
      ss_mutex_unlock (&_fd_lock);
      return -1;
    }

  switch (whence)
    {
    case SEEK_SET:
      fd->pos = pos;
      break;
    case SEEK_CUR:
      fd->pos += pos;
      break;
    case SEEK_END:
      fd->pos = fd->size - pos;
      break;
    default:
      break;
    }

  if (fd->pos < 0)
    fd->pos = 0;

  pos = fd->pos;

  ss_mutex_unlock (&fd->lock);

  return pos;
}

_off_t
_DEFUN (_lseek_r, (ptr, fd, pos, whence),
     struct _reent *ptr _AND
     int fd _AND
     _off_t pos _AND
     int whence)
{
  return lseek (fd, pos, whence);
}
