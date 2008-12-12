#include <sys/types.h>
#include <unistd.h>

uid_t
getuid (void)
{
  return 0;
}

gid_t
getgid (void)
{
  return 0;
}

uid_t
geteuid (void)
{
  return 0;
}

gid_t
getegid (void)
{
  return 0;
}

int
getgroups (int count, gid_t *groups)
{
  return 0;
}
