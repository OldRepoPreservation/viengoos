#include "setjmp.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

const char program_name[] = "t-setjmp";

static jmp_buf buf;

static int
recur (int a, int b)
{
  if (a + b == 0)
    _longjmp (buf, 23);

  return recur (a - 1, b);
}

static int
crc (uintptr_t *p, int count)
{
  int v = 0;
  while (count)
    {
      v += *p;
      p ++;
      count --;
    }

  return v;
}

int
main (int argc, char *argv[])
{
  /* Put some data on the stack that the compile will not be able to
     optimize away.  */
  char string[128];
  snprintf (string, sizeof (string), "%s", argv[0]);
  int c = crc ((uintptr_t *) string, sizeof (string) / sizeof (uintptr_t));

  int ret = _setjmp (buf);
  if (ret == 0)
    /* First return.  */
    {
      recur (4, 5);
    }

  assert (ret == 23);
  assert (c == crc ((uintptr_t *) string,
		    sizeof (string) / sizeof (uintptr_t)));

  return 0;
}
