/* Reentrant versions of read system call. */

#include <reent.h>
#include <unistd.h>
#include <errno.h>

#include <hurd/rm.h>

_ssize_t
read (int fd, void *buf, size_t cnt)
{
  if (fd != 0)
    {
      errno = EBADF;
      return -1;
    }

  if (cnt == 0)
    return 0;

  struct io_buffer buffer;
  rm_read (cnt, &buffer);

  memcpy (buf, buffer.data, buffer.len);
  return buffer.len;
}

_ssize_t
_DEFUN (_read_r, (ptr, fd, buf, cnt),
     struct _reent *ptr _AND
     int fd _AND
     _PTR buf _AND
     size_t cnt)
{
  return read (fd, buf, cnt);
}
