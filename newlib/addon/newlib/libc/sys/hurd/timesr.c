#include <reent.h>
#include <time.h>
#include <sys/time.h>
#include <sys/times.h>
#include <errno.h>

clock_t
_DEFUN (_times_r, (ptr, ptms),
     struct _reent *ptr _AND
     struct tms *ptms)
{
  errno = EOPNOTSUPP;
  return (clock_t) (-1);
}
