#define _L4_TEST_MAIN
#include "t-environment.h"

#include <stdlib.h>
#include <hurd/stddef.h>
#include "list.h"
#include "output.h"

struct object_desc
{
  int age;
  struct list_node activity_lru;
};

LIST_CLASS(object_activity_lru, struct object_desc, activity_lru, true)

int output_debug = 0;

#define N 10
void
test (void)
{
  struct object_desc *descs[N];

  struct object_activity_lru_list list;
  object_activity_lru_list_init (&list);

  struct object_desc *p;
  int i;

  /* Add items with value 0 to N-1.  (Recall: items are added to the
     head of the list!)  */
  for (i = 0; i < N; i ++)
    {
      assert (object_activity_lru_list_count (&list) == i);

      descs[i] = calloc (sizeof (struct object_desc), 1);
      descs[i]->age = i;
      object_activity_lru_list_push (&list, descs[i]);

      int j = i;
      for (p = object_activity_lru_list_head (&list);
	   p; p = object_activity_lru_list_next (p))
	assert (p->age == j --);
      assert (j == -1);

      j = 0;
      for (p = object_activity_lru_list_tail (&list);
	   p; p = object_activity_lru_list_prev (p))
	assert (p->age == j ++);
      assert (j == i + 1);
    }
  assert (object_activity_lru_list_count (&list) == i);

  /* Unlink nodes 0, 2, 4, 6, 8.  (Leaving the list: 9 -> 7 -> 5 -> 3
     -> 1.)  */
  for (i = 0; i < N; i += 2)
    {
      object_activity_lru_list_unlink (&list, descs[i]);
      free (descs[i]);

      int j = N - 1;
      for (p = object_activity_lru_list_head (&list);
	   p; p = object_activity_lru_list_next (p))
	{
	  assert (p->age == j --);
	  if (j <= i)
	    j --;
	}
      assert (j == -1);
    }

  /* Unlink nodes 1, 3, 5, 7, 9.  (Leaving an empty list.)  */
  for (i = 1; i < N; i += 2)
    {
      object_activity_lru_list_unlink (&list, descs[i]);
      free (descs[i]);

      int j = N - 1;
      for (p = object_activity_lru_list_head (&list);
	   p; p = object_activity_lru_list_next (p))
	{
	  assert (p->age == j);
	  j -= 2;
	}
      assert (j == i);
    }

  /* A: 1 -> 2 -> 3.  */
  struct object_activity_lru_list a;
  object_activity_lru_list_init (&a);

  descs[3] = calloc (sizeof (struct object_desc), 1);
  descs[3]->age = 3;
  object_activity_lru_list_push (&a, descs[3]);
  descs[2] = calloc (sizeof (struct object_desc), 1);
  descs[2]->age = 2;
  object_activity_lru_list_push (&a, descs[2]);
  descs[1] = calloc (sizeof (struct object_desc), 1);
  descs[1]->age = 1;
  object_activity_lru_list_push (&a, descs[1]);

  assert (object_activity_lru_list_count (&a) == 3);

  /* B: 4 -> 5 -> 6.  */
  struct object_activity_lru_list b;
  object_activity_lru_list_init (&b);

  descs[6] = calloc (sizeof (struct object_desc), 1);
  descs[6]->age = 6;
  object_activity_lru_list_push (&b, descs[6]);
  descs[5] = calloc (sizeof (struct object_desc), 1);
  descs[5]->age = 5;
  object_activity_lru_list_push (&b, descs[5]);
  descs[4] = calloc (sizeof (struct object_desc), 1);
  descs[4]->age = 4;
  object_activity_lru_list_push (&b, descs[4]);

  assert (object_activity_lru_list_count (&b) == 3);

  /* Join 'em.  */
  object_activity_lru_list_join (&a, &b);
  assert (! object_activity_lru_list_head (&b));

  assert (object_activity_lru_list_count (&a) == 6);
  assert (object_activity_lru_list_count (&b) == 0);

  for (i = 1, p = object_activity_lru_list_head (&a);
       i < 7; i ++, p = object_activity_lru_list_next (p))
    assert (p && p->age == i);
  assert (! p);

  for (i = 6, p = object_activity_lru_list_tail (&a);
       i > 0; i --, p = object_activity_lru_list_prev (p))
    assert (p && p->age == i);
  assert (! p);

  /* Move a to b.  */
  object_activity_lru_list_move (&b, &a);
  assert (! object_activity_lru_list_head (&a));
  for (i = 1, p = object_activity_lru_list_head (&b);
       i < 7; i ++, p = object_activity_lru_list_next (p))
    assert (p && p->age == i);
  assert (! p);

  /* Remove some elements.  */
  for (i = 2; i < 7; i ++)
    {
      object_activity_lru_list_unlink (&b, descs[i]);
      free (descs[i]);

      assert (object_activity_lru_list_head (&b)->age == 1);

      p = object_activity_lru_list_head (&b);
      assert (p && p->age == 1);
      int j;

      for (j = i + 1, p = object_activity_lru_list_next (p);
	   j < 7;
	   j ++, p = object_activity_lru_list_next (p))
	assert (p && p->age == j);
      assert (! p);

      p = object_activity_lru_list_tail (&b);
      for (j = 6; j > i; j --, p = object_activity_lru_list_prev (p))
	assert (p && p->age == j);
      assert (p && p->age == 1);
      assert (! object_activity_lru_list_prev (p));
    }
}
