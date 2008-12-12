#include <unistd.h>
#include <l4.h>

unsigned int
sleep (unsigned int seconds)
{
  /* XXX: This should be interrupted if a signal is received.  */
  l4_sleep (l4_time_period (seconds * 1000 * 1000));
  return 0;
}
