#include <reent.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>

int
fcntl (int fd, int cmd, ...)
{
  errno = EOPNOTSUPP;
  return -1;
}

int
_DEFUN (_fcntl_r, (ptr, fd, cmd, arg),
     struct _reent *ptr _AND
     int fd _AND
     int cmd _AND
     int arg)
{
  return fcntl (fd, cmd, arg);
}
