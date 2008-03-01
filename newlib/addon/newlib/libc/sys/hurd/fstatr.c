#include <reent.h>
#include <unistd.h>
#include <sys/stat.h>
#include <errno.h>

int
fstat (int fd, struct stat *pstat)
{
  errno = EOPNOTSUPP;
  return -1;
}

int
_fstat_r (ptr, fd, pstat)
     struct _reent *ptr;
     int fd;
     struct stat *pstat;
{
  return fstat (fd, pstat);
}
