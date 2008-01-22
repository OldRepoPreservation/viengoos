/* Reentrant version of close system call.  */

#include <reent.h>
#include <errno.h>

int
_close_r (ptr, fd)
     struct _reent *ptr;
     int fd;
{
  errno = EOPNOTSUPP;
  return -1;
}
