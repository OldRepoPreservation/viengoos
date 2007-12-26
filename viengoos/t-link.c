#define _L4_TEST_MAIN
#include "t-environment.h"

#include <hurd/stddef.h>
#include "object.h"
#include "output.h"

ss_mutex_t lru_lock;
int output_debug = 0;

#define N 10
void
test (void)
{
  struct object_desc descs[N];

  struct object_desc *head = NULL;
  struct object_desc *p;
  int i;

  ss_mutex_lock (&lru_lock);

  /* Add items with value 0 to N-1.  (Recall: items are added to the
     head of the list!)  */
  for (i = 0; i < N; i ++)
    {
      descs[i].age = i;
      descs[i].activity_lru.next = descs[i].activity_lru.prev = NULL;
      object_activity_lru_push (&head, &descs[i]);

      int j = i;
      p = head;
      if (p)
	do
	  {
	    assert (p->age == j --);

	    p = p->activity_lru.next;
	  }
	while (p != head);
      assert (j == -1);
    }

  /* Unlink nodes 0, 2, 4, 6, 8.  (Leaving the list: 9 -> 7 -> 5 -> 3
     -> 1.)  */
  for (i = 0; i < N; i += 2)
    {
      object_activity_lru_unlink (&head, &descs[i]);

      int j = N - 1;
      p = head;
      if (p)
	{
	  do
	    {
	      assert (p->age == j --);
	      if (j <= i)
		j --;

	      p = p->activity_lru.next;
	    }
	  while (p != head);
	}
      assert (j == -1);
    }

  /* Unlink nodes 1, 3, 5, 7, 9.  (Leaving an empty list.)  */
  for (i = 1; i < N; i += 2)
    {
      object_activity_lru_unlink (&head, &descs[i]);

      int j = N - 1;
      p = head;
      if (p)
	{
	  do
	    {
	      assert (p->age == j);
	      j -= 2;

	      p = p->activity_lru.next;
	    }
	  while (p != head);
	}
      assert (j == i);
    }

  /* A: 1 -> 2 -> 3.  */
  struct object_desc *a = NULL;
  object_activity_lru_push (&a, &descs[3]);
  object_activity_lru_push (&a, &descs[2]);
  object_activity_lru_push (&a, &descs[1]);
  /* B: 4 -> 5 -> 6.  */
  struct object_desc *b = NULL;
  object_activity_lru_push (&b, &descs[6]);
  object_activity_lru_push (&b, &descs[5]);
  object_activity_lru_push (&b, &descs[4]);

  /* Join 'em.  */
  object_activity_lru_join (&a, &b);
  assert (! b);
  for (i = 1, p = a; i < 7; i ++, p = p->activity_lru.next)
    assert (p && p->age == i);
  assert (p == a);

  /* Move a to b.  */
  object_activity_lru_move (&b, &a);
  assert (! a);
  for (i = 1, p = b; i < 7; i ++, p = p->activity_lru.next)
    assert (p && p->age == i);
  assert (p == b);

  /* Remove some elements.  */
  for (i = 2; i < 7; i ++)
    {
      object_activity_lru_unlink (&b, &descs[i]);

      assert (b->age == 1);
      int j;
      for (j = i + 1, p = b->activity_lru.next; j < 7;
	   j ++, p = p->activity_lru.next)
	assert (p && p->age == j);
      assert (p == b);
    }

  ss_mutex_unlock (&lru_lock);
}
