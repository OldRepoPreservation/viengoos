#include <reent.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>

int
_DEFUN (_fcntl_r, (ptr, fd, cmd, arg),
     struct _reent *ptr _AND
     int fd _AND
     int cmd _AND
     int arg)
{
  errno = EOPNOTSUPP;
  return -1;
}
