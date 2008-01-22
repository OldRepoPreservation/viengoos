/* Reentrant version of rename system call.  */

#include <reent.h>
#include <unistd.h>
#include <sys/stat.h>
#include <errno.h>

int
_DEFUN (_rename_r, (ptr, old, new),
     struct _reent *ptr _AND
     _CONST char *old _AND
     _CONST char *new)
{
  errno = EOPNOTSUPP;
  return -1;
}
