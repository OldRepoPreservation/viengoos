#include <reent.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>

#include "fd.h"

int
fcntl (int fdi, int cmd, ...)
{
  if (fdi < 0 || fdi >= _fd_size)
    {
      errno = EBADF;
      return -1;
    }

  ss_mutex_lock (&_fd_lock);

  struct _fd *fd = _fds[fdi];

  ss_mutex_unlock (&_fd_lock);

  /* XXX: Just say that we support it for now.  */

  return 0;
}

int
_DEFUN (_fcntl_r, (ptr, fd, cmd, arg),
     struct _reent *ptr _AND
     int fd _AND
     int cmd _AND
     int arg)
{
  return fcntl (fd, cmd, arg);
}
