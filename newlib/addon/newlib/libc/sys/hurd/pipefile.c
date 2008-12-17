#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <viengoos/misc.h>

#include "fd.h"

struct pipefile
{
  struct _fd fd;
};

static _ssize_t
pipe_pread (struct _fd *fd, void *buf, size_t size, off_t offset)
{
  struct io_buffer buffer;
  rm_read (VG_ADDR_VOID, VG_ADDR_VOID, size, &buffer);

  memcpy (buf, buffer.data, buffer.len);
  return buffer.len;
}

static int
pipe_ftruncate (struct _fd *fd, off_t length)
{
  errno = EPIPE;
  return -1;
}

static void
io_buffer_flush (struct io_buffer *buffer)
{
  if (buffer->len == 0)
    return;

  rm_write (VG_ADDR_VOID, VG_ADDR_VOID, *buffer);
  buffer->len = 0;
}

static void
io_buffer_append (struct io_buffer *buffer, int chr)
{
  if (buffer->len == sizeof (buffer->data))
    io_buffer_flush (buffer);

  buffer->data[buffer->len ++] = chr;
}

static _ssize_t
pipe_pwrite (struct _fd *fd, void *buf, size_t size, off_t offset)
{
  struct io_buffer buffer;
  buffer.len = 0;

  int i;
  for (i = 0; i < size; i ++)
    io_buffer_append (&buffer, ((char *) buf)[i]);
  io_buffer_flush (&buffer);

  return size;
}

static void
pipe_close (struct _fd *fd)
{
  struct pipefile *pipefile = (void *) fd;

  if (fd != &_stdio)
    free (pipefile);
}

static struct _fileops ops =
  {
    .pread = pipe_pread,
    .pwrite = pipe_pwrite,
    .ftruncate = pipe_ftruncate,
    .close = pipe_close,
  };

struct _fd _stdio = { 0, 0, -1, &ops };

struct _fd *
pipefile_open (const char *filename, int access)
{
  if (strcmp (filename, "/dev/stdin") != 0
      && strcmp (filename, "/dev/stdout") != 0
      && strcmp (filename, "/dev/stderr") != 0)
    return NULL;

  return &_stdio;
}

