#include <reent.h>
#include <unistd.h>
#include <sys/stat.h>
#include <errno.h>

int
_fstat_r (ptr, fd, pstat)
     struct _reent *ptr;
     int fd;
     struct stat *pstat;
{
  errno = EOPNOTSUPP;
  return -1;
}
