#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <errno.h>
#include <stddef.h>

#include "btree.h"

// #define DEBUG
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

BTREE_NODE_CLASS(int_node, struct int_node, int, key, node, int_node_compare)

static int
int_node_compare (const int *a, const int *b)
{
  return *a - *b;
}

void
print_node (struct int_node *a)
{
  printf ("%d%s(%d)", a->key, a->node.red ? "r" : "b",
	  a->node.parent ? ((struct int_node *)a->node.parent)->key : -1);
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
	print_nodes ((struct int_node *) a->node.left, depth - 1);
      else
	printf (".");
      printf ("<");
      print_node (a);
      printf (">");
      if (depth > 0)
	print_nodes ((struct int_node *) hurd_btree_link_internal (a->node.right),
		     depth - 1);
      else
	printf (".");
      printf (" %d}", depth);
    }
}

#define A 0
#define B 5000
#define C 10000

int
main (int argc, char *argv[])
{
  error_t err;
  hurd_btree_int_node_t root;
  struct int_node *node, *b;
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
	  err = hurd_btree_int_node_insert (&root, node);

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
      err = hurd_btree_int_node_insert (&root, node);
      assert (! err);
      err = hurd_btree_int_node_insert (&root, node);
      assert (err);
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
      err = hurd_btree_int_node_insert (&root, node);
      assert (! err);
      err = hurd_btree_int_node_insert (&root, node);
      assert (err);
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
      err = hurd_btree_int_node_insert (&root, node);
      assert (! err);
      err = hurd_btree_int_node_insert (&root, node);
      assert (err);
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
      err = hurd_btree_int_node_insert (&root, node);
      assert (! err);
      err = hurd_btree_int_node_insert (&root, node);
      assert (err);
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
      err = hurd_btree_int_node_insert (&root, node);

      /* Even are present, odd are not.  */
      if ((i & 1) == (A & 1))
	assert (err);
      else
	assert (! err);

      err = hurd_btree_int_node_insert (&root, node);
      assert (err);
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

  printf (". done\n"); fflush (stdout);

  return 0;
}


