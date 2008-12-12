#include <string.h>
#include <sys/resource.h>

int
getrusage (int processes, struct rusage *rusage)
{
  memset (rusage, 0, sizeof (*rusage));
  return 0;
}
