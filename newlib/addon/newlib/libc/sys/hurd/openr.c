/* Reentrant versions of open system call. */

#include <reent.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>

#include <hurd/mutex.h>

#include "fd.h"

ss_mutex_t _fd_lock;

static struct _fd *static_fds[] = { &_stdio, &_stdio, &_stdio, 0, 0, 0 };

struct _fd **_fds = static_fds;
int _fd_size = sizeof (static_fds) / sizeof (static_fds[0]);

int
open (const char *file, int flags, ...)
{
  struct _fd *fd;
  fd = pipefile_open (file, flags);
  if (! fd)
    fd = memfile_open (file, flags);
  if (! fd)
    {
      errno = ENOENT;
      return -1;
    }

  ss_mutex_lock (&_fd_lock);

  int i;
  for (i = 0; i < _fd_size; i ++)
    if (_fds[i] == NULL)
      break;

  if (i == _fd_size)
    /* Not large enough.  */
    {
      debug (5, "Growing fd array (%d)", _fd_size);

      if (_fds == static_fds)
	{
	  _fds = malloc (sizeof (_fds[0]) * _fd_size * 2);
	  if (_fds)
	    memcpy (_fds, static_fds, sizeof (static_fds));
	}
      else
	_fds = realloc (_fds, sizeof (_fds[0]) * _fd_size * 2);

      if (! _fds)
	{
	  debug (0, "Out of memory.");
	  abort ();
	}

      memset (&_fds[_fd_size], 0, sizeof (_fds[0]) * _fd_size);

      _fd_size *= 2;
    }

  debug (5, "Allocating fd %d", i);

  _fds[i] = fd;

  ss_mutex_unlock (&_fd_lock);

  return i;
}

int
_DEFUN (_open_r, (ptr, file, flags, mode),
     struct _reent *ptr _AND
     _CONST char *file _AND
     int flags _AND
     int mode)
{
  return open (file, flags, mode);
}

