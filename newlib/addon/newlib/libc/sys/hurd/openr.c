/* Reentrant versions of open system call. */

#include <reent.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

int
open (const char *file, int flags, ...)
{
  errno = EOPNOTSUPP;
  return -1;
}

int
_DEFUN (_open_r, (ptr, file, flags, mode),
     struct _reent *ptr _AND
     _CONST char *file _AND
     int flags _AND
     int mode)
{
  return open (file, flags, mode);
}

