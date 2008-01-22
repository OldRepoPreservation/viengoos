/* Reentrant version of sbrk system call. */

#include <reent.h>
#include <unistd.h>
#include <errno.h>

void *
_DEFUN (_sbrk_r, (ptr, incr),
     struct _reent *ptr _AND
     ptrdiff_t incr)
{
  /* XXX: This relies on the sbrk implementation being compiled
     against Newlib, otherwise, errno won't be propagated on
     error.  */
  void *sbrk (ptrdiff_t);
  return sbrk (incr);
}
