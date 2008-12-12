#include <unistd.h>
#include <errno.h>

char *
getcwd (char *buffer, size_t size)
{
  if (size < 2)
    {
      errno = ERANGE;
      return NULL;
    }

  buffer[0] = '/';
  buffer[1] = 0;
  return buffer;
}
