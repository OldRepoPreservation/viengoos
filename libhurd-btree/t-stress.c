#define _GNU_SOURCE

#include <assert.h>
#ifndef assertx
#define assertx(__ax_expr, __ax_fmt, ...)		\
  do							\
    {							\
      if (! (__ax_expr))				\
	printf (__ax_fmt, ##__VA_ARGS__);		\
      assert (__ax_expr);				\
    }							\
  while (0)
#endif

#include <stdlib.h>

#include "btree.h"

static int
int_node_compare (const int *a, const int *b)
{
  return *a - *b;
}

struct int_node
{
  struct hurd_btree_node node;
  int key;
};

BTREE_CLASS(int_node, struct int_node, int, key, node, int_node_compare, false)

int
main ()
{
  hurd_btree_int_node_t root;
  hurd_btree_int_node_tree_init (&root);

  int total = 0;
  while (1)
    {
      if (total % 1000 == 0)
	printf ("Added %d\n", total);
      total ++;

      struct int_node *node = node = malloc (sizeof (struct int_node));
      assert (node);

      node->key = rand ();

      struct int_node *ret = hurd_btree_int_node_insert (&root, node);
      if (ret)
	{
	  hurd_btree_int_node_detach (&root, ret);
	  free (ret);

	  ret = hurd_btree_int_node_insert (&root, node);
	  assert (! ret);
	}
    }

  return 0;
}
