#include <hurd/stddef.h>
#include "output.h"

static const char program_name[] = "t-link";

struct object_desc
{
  struct
  {
    struct object_desc *next;
    struct object_desc **prevp;
  } foo;
  int value;
};

LINK_TEMPLATE(foo)

#define N 10
int
main (int argc, char *argv[])
{
  struct object_desc descs[N];

  struct object_desc *head = NULL;
  struct object_desc *p;
  int i;

  /* Add items with value 0 to N-1.  (Recall: items are added to the
     head of the list!)  */
  for (i = 0; i < N; i ++)
    {
      descs[i].value = i;
      object_foo_link (&head, &descs[i]);

      int j;
      for (j = i, p = head; p; p = p->foo.next)
	assert (p->value == j --);
      assert (j == -1);
    }

  /* Unlink nodes 0, 2, 4, 6, 8.  (Leaving the list: 9 -> 7 -> 5 -> 3
     -> 1.)  */
  for (i = 0; i < N; i += 2)
    {
      object_foo_unlink (&descs[i]);

      int j;
      for (j = N-1, p = head; p; p = p->foo.next)
	{
	  assert (p->value == j --);
	  if (j <= i)
	    j --;
	}
      assert (j == -1);
    }

  /* A: 1 -> 2 -> 3.  */
  struct object_desc *a = NULL;
  object_foo_link (&a, &descs[3]);
  object_foo_link (&a, &descs[2]);
  object_foo_link (&a, &descs[1]);
  /* B: 4 -> 5 -> 6.  */
  struct object_desc *b = NULL;
  object_foo_link (&b, &descs[6]);
  object_foo_link (&b, &descs[5]);
  object_foo_link (&b, &descs[4]);

  /* Join 'em.  */
  object_foo_join (&a, &b);
  assert (! b);
  for (i = 1, p = a; i < 7; i ++, p = p->foo.next)
    assert (p && p->value == i);
  assert (! p);

  /* Move a to b.  */
  object_foo_move (&b, &a);
  assert (! a);
  for (i = 1, p = b; i < 7; i ++, p = p->foo.next)
    assert (p && p->value == i);
  assert (! p);

  /* Remove some elements.  */
  for (i = 2; i < 7; i ++)
    {
      object_foo_unlink (&descs[i]);

      assert (b->value == 1);
      int j;
      for (j = i + 1, p = b->foo.next; j < 7; j ++, p = p->foo.next)
	assert (p && p->value == j);
      assert (! p);
    }

  return 0;
}