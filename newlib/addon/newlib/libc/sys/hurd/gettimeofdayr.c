#include <reent.h>
#include <time.h>
#include <sys/time.h>
#include <sys/times.h>
#include <errno.h>

#include <l4/schedule.h>

int
gettimeofday (struct timeval *ptimeval, void *ptimezone)
{
  /* This implementation is useful for measurements.  */
  l4_clock_t t = l4_system_clock ();

  ptimeval->tv_sec = t / 1000000;
  ptimeval->tv_usec = t % 1000000;

  return 0;
}

int
_DEFUN (_gettimeofday_r, (ptr, ptimeval, ptimezone),
     struct _reent *ptr _AND
     struct timeval *ptimeval _AND
     void *ptimezone)
{
  return gettimeofday (ptimeval, ptimezone);
}
