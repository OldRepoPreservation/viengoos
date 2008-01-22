/* Reentrant versions of read system call. */

#include <reent.h>
#include <unistd.h>
#include <errno.h>

_ssize_t
_DEFUN (_read_r, (ptr, fd, buf, cnt),
     struct _reent *ptr _AND
     int fd _AND
     _PTR buf _AND
     size_t cnt)
{
  errno = EOPNOTSUPP;
  return -1;
}
