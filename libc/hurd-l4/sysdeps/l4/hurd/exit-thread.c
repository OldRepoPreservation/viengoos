#include <unistd.h>

/* Special exit function which only terminates the current thread.  */
void
__exit_thread (int val)
{
  /* FIXME: Terminate ourselves.  */
  while (1);
}

stub_warning (__exit_thread)
#include <stub-tag.h>
