#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "bit-array.h"

static unsigned char array[16];

int
main (int argc, char *argv[])
{
  /* Make sure we can set all the bits when the bit to set is the one
     under the hint.  */
  int i;
  for (i = 0; i < sizeof (array) * 8; i ++)
    assert (bit_alloc (array, sizeof (array), i) == i);
  assert (bit_alloc (array, sizeof (array), 1) == -1);

  memset (array, 0, sizeof (array));

  /* Make sure we can set all the bits when the bit to set is not
     (necessarily) under the hint.  */
  for (i = 0; i < sizeof (array) * 8; i ++)
    {
      int b = (10 + i) % (sizeof (array) * 8);
      assert (bit_alloc (array, sizeof (array), 10) == b);
    }
  assert (bit_alloc (array, sizeof (array), 1) == -1);

  /* Clear one bit and make sure that independent of the start hint,
     we find that bit.  */
  for (i = 0; i < sizeof (array) * 8; i ++)
    {
      bit_dealloc (array, 75);

      assert (bit_alloc (array, sizeof (array), i) == 75);
      assert (bit_alloc (array, sizeof (array), i) == -1);
    }

  /* See if we can set a bit in the middle of a byte.  */
  bit_dealloc (array, 11);
  assert (bit_set (array, sizeof (array), 11) == true);
  assert (bit_set (array, sizeof (array), 11) == false);

  /* And at the start of a byte.  */
  bit_dealloc (array, 24);
  assert (bit_set (array, sizeof (array), 24) == true);
  assert (bit_set (array, sizeof (array), 24) == false);

  return 0;
}
