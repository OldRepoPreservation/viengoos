/* Reentrant version of rename system call.  */

#include <reent.h>
#include <unistd.h>
#include <sys/stat.h>
#include <errno.h>

int
rename (const char *old, const char *new)
{
  errno = EOPNOTSUPP;
  return -1;
}

int
_DEFUN (_rename_r, (ptr, old, new),
     struct _reent *ptr _AND
     _CONST char *old _AND
     _CONST char *new)
{
  return rename (old, new);
}
