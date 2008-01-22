#include <reent.h>
#include <unistd.h>
#include <errno.h>

int
_DEFUN (_unlink_r, (ptr, file),
     struct _reent *ptr _AND
     _CONST char *file)
{
  errno = EOPNOTSUPP;
  return -1;
}
