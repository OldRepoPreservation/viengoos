#include <reent.h>
#include <unistd.h>
#include <errno.h>

int
_DEFUN (_link_r, (ptr, old, new),
     struct _reent *ptr _AND
     _CONST char *old _AND
     _CONST char *new)
{
  errno = EOPNOTSUPP;
  return -1;
}
