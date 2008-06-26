#include <reent.h>
#include <unistd.h>
#include <errno.h>

#include <hurd/rm.h>

static void
io_buffer_flush (struct io_buffer *buffer)
{
  if (buffer->len == 0)
    return;

  rm_write (*buffer);
  buffer->len = 0;
}

static void
io_buffer_append (struct io_buffer *buffer, int chr)
{
  if (buffer->len == sizeof (buffer->data))
    io_buffer_flush (buffer);

  buffer->data[buffer->len ++] = chr;
}

_ssize_t
write (int fd, const void *buf, size_t cnt)
{
  if (fd == 1 || fd == 2)
    {
      struct io_buffer buffer;
      buffer.len = 0;

      int i;
      for (i = 0; i < cnt; i ++)
	io_buffer_append (&buffer, ((char *) buf)[i]);
      io_buffer_flush (&buffer);

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
