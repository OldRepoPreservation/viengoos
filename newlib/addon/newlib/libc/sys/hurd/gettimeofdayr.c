#include <reent.h>
#include <time.h>
#include <sys/time.h>
#include <sys/times.h>
#include <errno.h>

int
_DEFUN (_gettimeofday_r, (ptr, ptimeval, ptimezone),
     struct _reent *ptr _AND
     struct timeval *ptimeval _AND
     void *ptimezone)
{
  errno = EOPNOTSUPP;
  return -1;
}
