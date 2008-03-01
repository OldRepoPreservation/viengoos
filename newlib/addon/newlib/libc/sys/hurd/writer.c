#include <reent.h>
#include <unistd.h>
#include <errno.h>

#include <hurd/rm.h>

_ssize_t
write (int fd, const void *buf, size_t cnt)
{
  if (fd == 1 || fd == 2)
    {
      int i;
      for (i = 0; i < cnt; i ++)
	rm_putchar (((char *) buf)[i]);

      return cnt;
    }

  errno = EOPNOTSUPP;
  return -1;
}


_ssize_t
_DEFUN (_write_r, (ptr, fd, buf, cnt),
     struct _reent *ptr _AND
     int fd _AND
     _CONST _PTR buf _AND
     size_t cnt)
{
  return write (fd, buf, cnt);
}
