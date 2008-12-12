#include <pwd.h>

struct passwd *
getpwuid (uid_t uid)
{
  if (uid != 0)
    return NULL;

  static struct passwd passwd;

  passwd.pw_name = "root";
  passwd.pw_passwd = "root";
  passwd.pw_uid = 0;
  passwd.pw_gid = 0;
  passwd.pw_gecos = "root";
  passwd.pw_dir = "/";
  passwd.pw_shell = NULL;

  return &passwd;
}
