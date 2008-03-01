/* Reentrant versions of syscalls need to support signal/raise.
   These implementations just call the usual system calls.  */

#include <reent.h>
#include <signal.h>
#include <unistd.h>
#include <errno.h>

int
_DEFUN (_kill_r, (ptr, pid, sig),
     struct _reent *ptr _AND
     int pid _AND
     int sig)
{
  return kill (pid, sig);
}

int
getpid (void)
{
  return 1032;
}

int
_DEFUN (_getpid_r, (ptr),
     struct _reent *ptr)
{
  return getpid ();
}
