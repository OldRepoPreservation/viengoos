#ifndef HURD_FD

#include <hurd/mutex.h>
#include <unistd.h>
#include <sys/types.h>

struct _fd;

struct _fileops
{
  void (*close) (struct _fd *fd);
  _ssize_t (*pread) (struct _fd *fd, void *buf, size_t size, off_t offset);
  _ssize_t (*pwrite) (struct _fd *fd, void *buf, size_t size, off_t offset);
  int (*ftruncate) (struct _fd *fd, off_t length);
};

struct _fd
{
  ss_mutex_t lock;
  int pos;
  /* -1 if its a pipe.  */
  int size;
  struct _fileops *ops;
};

extern ss_mutex_t _fd_lock;
extern struct _fd **_fds;
/* Number of elements in _FILES.  */
extern int _fd_size;


extern struct _fd *memfile_open (const char *filename, int access);
extern struct _fd *pipefile_open (const char *filename, int access);

extern struct _fd _stdio;

#endif
