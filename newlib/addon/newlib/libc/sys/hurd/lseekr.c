#include <reent.h>
#include <unistd.h>
#include <errno.h>

_off_t
lseek (int fd, _off_t pos, int whence)
{
  errno = EOPNOTSUPP;
  return -1;
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
