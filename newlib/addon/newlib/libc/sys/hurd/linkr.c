#include <reent.h>
#include <unistd.h>
#include <errno.h>

int
link (const char *old, const char *new)
{
  errno = EOPNOTSUPP;
  return -1;
}

int
_DEFUN (_link_r, (ptr, old, new),
     struct _reent *ptr _AND
     _CONST char *old _AND
     _CONST char *new)
{
  return link (old, new);
}
