/* Reentrant version of close system call.  */

#include <reent.h>
#include <errno.h>

int
close (int fd)
{
  errno = EOPNOTSUPP;
  return -1;
}

int
_close_r (ptr, fd)
     struct _reent *ptr;
     int fd;
{
  return close (fd);
}
