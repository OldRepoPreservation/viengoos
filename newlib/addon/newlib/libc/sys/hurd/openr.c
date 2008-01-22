/* Reentrant versions of open system call. */

#include <reent.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

int
_DEFUN (_open_r, (ptr, file, flags, mode),
     struct _reent *ptr _AND
     _CONST char *file _AND
     int flags _AND
     int mode)
{
  errno = EOPNOTSUPP;
  return -1;
}
