#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>

#include "fd.h"

struct memfile
{
  struct _fd fd;

  /* Bytes allocated.  */
  _ssize_t alloced;
  void *data;
  char *filename;
};

static _ssize_t
mem_pread (struct _fd *fd, void *buf, size_t size, off_t offset)
{
  struct memfile *memfile = (void *) fd;

  debug (5, "%s(%d) (%p, %d, %d)",
	 memfile->filename, fd->size, buf, size, offset);

  if (offset >= fd->size)
    return 0;

  if (offset + size > fd->size)
    size = fd->size - offset;

  memcpy (buf, memfile->data + offset, size);

  return size;
}

static int
mem_ftruncate (struct _fd *fd, off_t length)
{
  struct memfile *memfile = (void *) fd;

  debug (5, "%s(%d) (%d)", memfile->filename, fd->size, length);

  if (length > memfile->alloced)
    {
      int a = memfile->alloced;

      if (memfile->alloced == 0)
	memfile->alloced = PAGESIZE;
      while (length > memfile->alloced)
	memfile->alloced *= 2;

      debug (5, "Growing from %d to %d bytes", a, memfile->alloced);

      memfile->data = realloc (memfile->data, memfile->alloced);
      if (! memfile->data)
	{
	  debug (0, "Out of memory");
	  abort ();
	}

      memset (memfile->data + a, 0, memfile->alloced - a);
    }

  fd->size = length;

  return 0;
}

static _ssize_t
mem_pwrite (struct _fd *fd, void *buf, size_t size, off_t offset)
{
  struct memfile *memfile = (void *) fd;

  debug (5, "%s(%d) (%p, %d, %d)",
	 memfile->filename, fd->size, buf, size, offset);

  if (offset + size > fd->size)
    mem_ftruncate (fd, offset + size);

  memcpy (memfile->data + offset, buf, size);

  return size;
}

static void
mem_close (struct _fd *fd)
{
  struct memfile *memfile = (void *) fd;

  debug (0, "%s(%d)", memfile->filename, fd->size);

  free (memfile->data);
  free (memfile->filename);
  free (memfile);
}

static struct _fileops ops =
  {
    .pread = mem_pread,
    .pwrite = mem_pwrite,
    .ftruncate = mem_ftruncate,
    .close = mem_close,
  };

struct _fd *
memfile_open (const char *filename, int access)
{
  struct memfile *memfile = calloc (sizeof (struct memfile), 1);

  memfile->fd.ops = &ops;

  memfile->filename = strdup (filename);

  return &memfile->fd;
}

