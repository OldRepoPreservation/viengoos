#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <errno.h>
#include <stddef.h>

#include "btree.h"

char *program_name = "btree-test";

// #define DEBUG
#undef debug
#ifdef DEBUG
#define debug(fmt, ...) printf (fmt, ##__VA_ARGS__)
#else
#define debug(fmt, ...) do { } while (0)
#endif

static int int_node_compare (const int *a, const int *b);

struct int_node
{
  struct hurd_btree_node node;
  int key;
};

BTREE_CLASS(int_node, struct int_node, int, key, node, int_node_compare, false)

BTREE_CLASS(intd_node, struct int_node, int, key, node, int_node_compare, true)

static int
int_node_compare (const int *a, const int *b)
{
  return *a - *b;
}

void
print_node (struct int_node *a)
{
  printf ("%d%s(%d)", a->key, BTREE_NODE_RED_P (&a->node) ? "r" : "b",
	  BTREE_NP (a->node.parent)
	  ? ((struct int_node *) BTREE_NP (a->node.parent))->key : -1);
}

void
print_nodes (struct int_node *a, int depth)
{
  if (! a)
    printf ("*");
  else
    {
      printf ("{%d ", depth);
      if (depth > 0)
	print_nodes ((struct int_node *) BTREE_NP_CHILD (a->node.left),
		     depth - 1);
      else
	printf (".");
      printf ("<");
      print_node (a);
      printf (">");
      if (depth > 0)
	print_nodes ((struct int_node *) BTREE_NP_CHILD (a->node.right),
		     depth - 1);
      else
	printf (".");
      printf (" %d}", depth);
    }
}

#define A 0
#define B 500
#define C 1000

int
main (int argc, char *argv[])
{
  hurd_btree_int_node_t root;
  struct int_node *node, *b;
  struct int_node *ret;
  int i, j, k, m;
  int a[] = { 16, 18, 17, 1, 15, 12, 8, 9, 10, 3, 4, 11, 21, 20, 19,
	      6, 5, 14, 13, 24, 23, 22, 7, 2 };

  hurd_btree_int_node_tree_init (&root);

  node = hurd_btree_int_node_first (&root);
  assert (! node);

  /* Insert the elements in the array A.  */
  for (m = 0; m < sizeof (a) / sizeof (*a); m ++)
    {
      for (i = m; i < sizeof (a) / sizeof (*a); i ++)
	{
	  node = malloc (sizeof (struct int_node));
	  assert (node);
	  node->key = a[i];
	  debug ("Inserting %d... ", a[i]);
	  fflush (stdout);
	  ret = hurd_btree_int_node_insert (&root, node);

	  fflush (stdout);
	  node = hurd_btree_int_node_find (&root, &a[i]);
	  assert (node);
	  assert (node->key == a[i]);

	  node = hurd_btree_int_node_first (&root);
	  k = node->key - 1;
	  for (j = m; j <= i; j ++)
	    {
	      assert (node);
	      assert (k < node->key);
	      k = node->key;
	      node = hurd_btree_int_node_next (node);
	    }
	  assert (! node);
	  debug ("done\n");
	}

      /* Detach the elements in the array A.  */
      for (i = m; i < sizeof (a) / sizeof (*a); i ++)
	{
	  debug ("Searching for %d... ", a[i]);
	  fflush (stdout);
	  node = hurd_btree_int_node_find (&root, &a[i]);
	  assert (node);
	  assert (node->key == a[i]);

	  debug ("detaching... ");
	  fflush (stdout);
	  hurd_btree_int_node_detach (&root, node);
	  free (node);

	  debug ("verifying... ");
	  fflush (stdout);
	  node = hurd_btree_int_node_find (&root, &a[i]);
	  assert (! node);

	  node = hurd_btree_int_node_first (&root);
	  for (k = i + 1; k < sizeof (a) / sizeof (*a); k ++)
	    {
	      assert (node);
	      assert (node->key <= a[k]);
	    }
	  k = 0;
	  for (j = i + 1; j < sizeof (a) / sizeof (*a); j ++)
	    {
	      assert (node);
	      assert (k < node->key);
	      k = node->key;
	      node = hurd_btree_int_node_next (node);
	    }
	  assert (! node);
	  debug ("done\n");
	}
    }

  printf ("."); fflush (stdout);

  /* Insert elements { B, B + 2, ..., C }.  */
  for (i = B; i <= C; i += 2)
    {
      node = malloc (sizeof (struct int_node));
      assert (node);

      node->key = i;
      debug ("Inserting %d... ", i);
      fflush (stdout);
      ret = hurd_btree_int_node_insert (&root, node);
      assert (! ret);
      ret = hurd_btree_int_node_insert (&root, node);
      assert (ret);
      debug ("done\n");

      node = hurd_btree_int_node_first (&root);
      for (j = B; j <= i; j += 2)
	{
	  assert (node);
	  assert (node->key == j);
	  node = hurd_btree_int_node_next (node);
	}
      assert (! node);
    }

  printf ("."); fflush (stdout);

  /* Detach the elements { B, B + 2, ..., C }.  */
  for (i = B; i <= C; i += 2)
    {
      debug ("Searching for %d... ", i);
      fflush (stdout);
      node = hurd_btree_int_node_find (&root, &i);
      assert (node);
      assert (node->key == i);

      debug ("detaching... ");
      fflush (stdout);
      hurd_btree_int_node_detach (&root, node);
      free (node);

      debug ("verifying... ");
      fflush (stdout);
      node = hurd_btree_int_node_find (&root, &i);
      assert (! node);

      node = hurd_btree_int_node_first (&root);
      for (j = i + 2; j <= C; j += 2)
	{
	  assert (node);
	  assert (node->key == j);
	  node = hurd_btree_int_node_next (node);
	}
      assert (! node);

      debug ("done\n");
    }

  printf ("."); fflush (stdout);

  /* The tree should be empty now.  */
  node = hurd_btree_int_node_first (&root);
  assert (! node);

  /* Add elements { C, C - 2, ..., A }.  */
  for (i = C; i >= A; i -= 2)
    {
      node = malloc (sizeof (struct int_node));
      assert (node);

      node->key = i;
      debug ("Inserting %d... ", i);
      fflush (stdout);
      ret = hurd_btree_int_node_insert (&root, node);
      assert (! ret);
      ret = hurd_btree_int_node_insert (&root, node);
      assert (ret);
      debug ("done\n");

      node = hurd_btree_int_node_first (&root);
      for (j = i; j <= C; j += 2)
	{
	  assert (node);
	  assert (node->key == j);
	  node = hurd_btree_int_node_next (node);
	}
      assert (! node);
    }

  printf ("."); fflush (stdout);

  /* Detach elements { B, B + 2, ..., C }.  */
  for (i = B; i <= C; i += 2)
    {
      debug ("Searching for %d... ", i);
      fflush (stdout);
      node = hurd_btree_int_node_find (&root, &i);
      assert (node);
      assert (node->key == i);

      debug ("detaching... ");
      fflush (stdout);
      hurd_btree_int_node_detach (&root, node);
      free (node);

      debug ("verifying... ");
      fflush (stdout);
      node = hurd_btree_int_node_find (&root, &i);
      assert (! node);

      node = hurd_btree_int_node_first (&root);
      for (j = A; j < B; j += 2)
	{
	  assert (node);
	  assert (node->key == j);
	  node = hurd_btree_int_node_next (node);
	}
      for (j = i + 2; j <= C; j += 2)
	{
	  assert (node);
	  assert (node->key == j);
	  node = hurd_btree_int_node_next (node);
	}
      assert (! node);

      debug ("done\n");
    }

  printf ("."); fflush (stdout);

  /* Detach elements { B - 2, B - 6, ..., A }.  */
  for (i = B - 2; i >= A; i -= 2)
    {
      debug ("Searching for %d... ", i);
      fflush (stdout);
      node = hurd_btree_int_node_find (&root, &i);
      assert (node);
      assert (node->key == i);

      debug ("detaching... ");
      fflush (stdout);
      hurd_btree_int_node_detach (&root, node);
      free (node);

      debug ("verifying... ");
      fflush (stdout);
      node = hurd_btree_int_node_find (&root, &i);
      assert (! node);

      node = hurd_btree_int_node_first (&root);
      for (j = A; j < i; j += 2)
	{
	  assert (node);
	  assert (node->key == j);
	  node = hurd_btree_int_node_next (node);
	}
      assert (! node);

      debug ("done\n");
    }

  printf ("."); fflush (stdout);

  /* Empty again.  */
  node = hurd_btree_int_node_first (&root);
  assert (! node);

  /* Insert { B - 2, B - 4, ..., A }.  */
  for (i = B - 2 ; i >= A; i -= 2)
    {
      node = malloc (sizeof (struct int_node));
      assert (node);

      node->key = i;
      debug ("Inserting %d... ", i);
      fflush (stdout);
      ret = hurd_btree_int_node_insert (&root, node);
      assert (! ret);
      ret = hurd_btree_int_node_insert (&root, node);
      assert (ret);
      debug ("done\n");

      node = hurd_btree_int_node_first (&root);
      for (j = i; j < B; j += 2)
	{
	  assert (node);
	  assert (node->key == j);
	  node = hurd_btree_int_node_next (node);
	}
      assert (! node);
    }

  printf ("."); fflush (stdout);

  /* Add { B, B + 2, ..., C }.  */
  for (i = B; i <= C; i += 2)
    {
      node = malloc (sizeof (struct int_node));
      assert (node);

      node->key = i;
      debug ("Inserting %d... ", i);
      fflush (stdout);
      ret = hurd_btree_int_node_insert (&root, node);
      assert (! ret);
      ret = hurd_btree_int_node_insert (&root, node);
      assert (ret);
      debug ("done\n");

      node = hurd_btree_int_node_first (&root);
      for (j = A; j <= i; j += 2)
	{
	  assert (node);
	  assert (node->key == j);
	  node = hurd_btree_int_node_next (node);
	}
      assert (! node);
    }

  printf ("."); fflush (stdout);

  /* Verify A -> C.  */
  for (i = A; i <= C; i ++)
    {
      debug ("Searching for %d... ", i);
      node = hurd_btree_int_node_find (&root, &i);
      b = hurd_btree_int_node_find (&root, &i);
      assert (node == b);

      /* { A, A + 2, ... C } are in, { A + 1, A + 3, ..., C - 1 } are
	 not.  */
      if ((i & 1) == (A & 1))
	{
	  assert (node);
	  assert (node->key == i);
	}
      else
	assert (! node);
      debug ("ok\n");
    }

  printf ("."); fflush (stdout);

  /* Now add the odds, i.e. { A + 1, A + 3, ..., C - 1 }.  */

  for (i = A; i <= C; i ++)
    {
      debug ("Inserting %d... ", i);
      node = malloc (sizeof (struct int_node));
      assert (node);

      node->key = i;
      ret = hurd_btree_int_node_insert (&root, node);

      /* Even are present, odd are not.  */
      if ((i & 1) == (A & 1))
	assert (ret);
      else
	assert (! ret);

      ret = hurd_btree_int_node_insert (&root, node);
      assert (ret);
      debug ("\n");
    }

  printf ("."); fflush (stdout);

  /* Verify that { A, A + 1, ..., C } are all present.  */
  for (i = A; i <= C; i ++)
    {
      debug ("Searching for %d... ", i);
      node = hurd_btree_int_node_find (&root, &i);
      b = hurd_btree_int_node_find (&root, &i);
      assert (node == b);
      assert (node);
      assert (node->key == i);
      debug ("ok\n");
    }

  printf ("."); fflush (stdout);

  /* Iterate over the nodes.  */
  node = hurd_btree_int_node_first (&root);
  for (i = A; i <= C; i ++)
    {
      assert (node);
      assert (node->key == i);
      node = hurd_btree_int_node_next (node);
    }
  assert (! node);

  printf ("."); fflush (stdout);

  /* Detach elements { A, A + 2, ..., C }.  */
  for (i = A; i <= C; i += 2)
    {
      debug ("Searching for %d... ", i);
      fflush (stdout);
      node = hurd_btree_int_node_find (&root, &i);
      assert (node);
      assert (node->key == i);

      debug ("detaching... ");
      fflush (stdout);
      hurd_btree_int_node_detach (&root, node);
      free (node);

      debug ("verifying... ");
      fflush (stdout);
      node = hurd_btree_int_node_find (&root, &i);
      assert (! node);

      node = hurd_btree_int_node_first (&root);
      for (j = A + 1; j < i; j += 2)
	{
	  assert (node);
	  assert (node->key == j);
	  node = hurd_btree_int_node_next (node);
	}
      for (j = i + 1; j <= C; j ++)
	{
	  assert (node);
	  assert (node->key == j);
	  node = hurd_btree_int_node_next (node);
	}
      assert (! node);

      debug ("done\n");
    }

  printf ("."); fflush (stdout);

  /* Detach elements { A + 1, A + 3, ..., C - 1 }.  */
  for (i = A + 1; i <= C; i += 2)
    {
      debug ("Searching for %d... ", i);
      fflush (stdout);
      node = hurd_btree_int_node_find (&root, &i);
      assert (node);
      assert (node->key == i);

      debug ("detaching... ");
      fflush (stdout);
      hurd_btree_int_node_detach (&root, node);
      free (node);

      debug ("verifying... ");
      fflush (stdout);
      node = hurd_btree_int_node_find (&root, &i);
      assert (! node);

      node = hurd_btree_int_node_first (&root);
      for (j = i + 2; j <= C; j += 2)
	{
	  assert (node);
	  assert (node->key == j);
	  node = hurd_btree_int_node_next (node);
	}
      assert (! node);

      debug ("done\n");
    }

  /* Empty again.  */
  node = hurd_btree_int_node_first (&root);
  assert (! node);

  /* Test whether we can insert nodes with the same value into a true
     with MAY_OVERLAP set to true.  */
  hurd_btree_intd_node_t droot;
  hurd_btree_intd_node_tree_init (&droot);

  node = hurd_btree_intd_node_first (&droot);
  assert (! node);

  /* Insert 0, 1, ..., max - 1, 10 times.  */
  const int max = 10;
  for (i = 0; i < 10; i ++)
    for (j = 0; j < max; j ++)
      {
	debug ("Inserting %d... ", j);
	node = malloc (sizeof (struct int_node));
	assert (node);

	node->key = j;
	ret = hurd_btree_intd_node_insert (&droot, node);
	assert (! ret);

	int cnt = 0;
	node = hurd_btree_intd_node_first (&droot);
	while (node)
	  {
	    /* Consider:

	       We've added:

	         0, 1, 2, 3, 4, 0, 1, 2, 3, 4, 0, 1, 2

	       thus, max = 4, and i = 2, j = 2.

	       As we traverse the list, we expect:
	       
	         0  1  2  3  4  5  6  7  8  9 10 11 12

	         0, 0, 0, 1, 1, 1, 2, 2, 2, 3, 3, 4, 4
		 \     /  \     /  \     /  \  /  \  /
		 i+1 0's  i+1 1's  i+1 2's  i 3's  i 4's

	       Thus, we expect that if CNT <= (i+1)*(j+1),

	         key(node(cnt)) = cnt / (i + 1)

	       e.g., 7 / (2 + 1) = 2, as expected

	       Otherwise,

	         key(node(cnt)) = j + 1 + ((cnt - ((i + 1) * (j + 1))) / i)

	       e.g., 2 + 1 + ((11 - ((2 + 1) * (2 + 1))) / 2) = 4
	    */

	    if (i == 0)
	      assert (node->key == cnt);
	    else if (cnt <= (i + 1) * (j + 1))
	      assert (node->key == cnt / (i + 1));
	    else
	      assert (node->key == j + 1 + ((cnt - ((i + 1) * (j + 1))) / i));
	    cnt ++;

	    struct int_node *next = hurd_btree_intd_node_next (node);
	    if (next)
	      assert (hurd_btree_intd_node_prev (next) == node);
	    node = next;
	  }
	assert (cnt == max * i + j + 1);
      }


  printf (". done\n"); fflush (stdout);

  return 0;
}


