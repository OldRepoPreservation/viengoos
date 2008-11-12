#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <errno.h>
#include <stddef.h>
#include <string.h>

#include "btree.h"

char *program_name = "btree-test";

int output_debug;

// #define DEBUG
#ifdef DEBUG
# ifndef debug
#  define debug(level, fmt, ...)				\
  if (level <= output_debug)					\
    {								\
      printf (fmt "\n", ##__VA_ARGS__);				\
      fflush (stddout);						\
    }
# else
#  define debug(level, fmt, ...) do { } while (0)
# endif
#endif

struct region
{
  uintptr_t start;
  uintptr_t length;
};

/* Compare two regions.  Two regions are considered equal if there is
   any overlap at all.  */
static int
region_compare (const struct region *a, const struct region *b)
{
  uintptr_t a_end = a->start + (a->length - 1);
  uintptr_t b_end = b->start + (b->length - 1);

  int ret = 0;
  if (a_end < b->start)
    ret = -1;
  if (a->start > b_end)
    ret = 1;

  debug (5, "%d+%d %s %d+%d\n",
	 a->start, a_end,
	 ret == 0 ? "==" : ret == -1 ? "<" : ">",
	 b->start, b_end);

  return ret;
}

struct region_node
{
  hurd_btree_node_t node;
  struct region region;
};

BTREE_CLASS(region_node, struct region_node, struct region, region, node,
	    region_compare, true)

void
print_node (struct region_node *a)
{
  printf ("%d+%d%s(%d)",
	  a->region.start, a->region.start + a->region.length - 1,
	  BTREE_NODE_RED_P (&a->node) ? "r" : "b",
	  BTREE_NP (a->node.parent)
	  ? ((struct region_node *) BTREE_NP (a->node.parent))->region.start
	  : -1);
}

bool
do_print_nodes (struct region_node *a, int depth)
{
  bool saw_one = false;
  if (depth > 0)
    {
      if (a)
	{
	  struct region_node *l, *r;
	  l = (struct region_node *) BTREE_NP_CHILD (a->node.left);
	  r = (struct region_node *) BTREE_NP_CHILD (a->node.right);

	  saw_one = do_print_nodes (l, depth - 1);
	  saw_one |= do_print_nodes (r, depth - 1);
	}
      else
	{
	  do_print_nodes (NULL, depth - 1);
	  do_print_nodes (NULL, depth - 1);
	}
    }
  else
    {
      if (a)
	{
	  print_node (a);
	  saw_one = true;
	}
      else
	printf ("NULL");
      printf (" ");
    }

  return saw_one;
}

void
print_nodes (struct region_node *a, int depth)
{
  int i;
  bool saw_one = true;
  for (i = 0; i < depth && saw_one; i ++)
    {
      saw_one = do_print_nodes (a, i);
      printf ("\n");
    }
}

int
main (int argc, char *argv[])
{
  hurd_btree_region_node_t root;
  hurd_btree_region_node_tree_init (&root);

  /* 0-5, 2-7, 4-9, ...  */
#define COUNT 100
#define LENGTH 5

  int i;
  for (i = 0; i < 100; i += 2)
    {
      struct region_node *node = calloc (sizeof (struct region_node), 1);
      assert (node);
      node->region.start = i;
      node->region.length = 5;
      struct region_node *conflict
	= hurd_btree_region_node_insert (&root, node);
      assert (! conflict);

      int j;
      for (j = 0; j <= i + 10; j ++)
	{
	  struct region region;
	  region.start = j;
	  region.length = 20;

	  node = hurd_btree_region_node_find_first (&root, &region);

	  if (node)
	    debug (5, "Searching for %d-%d and got %d-%d",
		   region.start, region.start + region.length - 1,
		   node->region.start,
		   node->region.start + node->region.length - 1);
	  else
	    debug (5, "Searching for %d-%d and got NULL",
		   region.start, region.start + region.length - 1);

	  if (j >= 0 && j <= i + LENGTH - 1)
	    {
	      int expect = j - LENGTH + 1;
	      if (expect < 0)
		expect = 0;
	      if (expect % 2 == 1)
		expect ++;

	      debug (5, "Expected %d-%d", expect, expect + LENGTH - 1);
	      if (! node || node->region.start != expect)
		{
		  print_nodes (BTREE_NP (root.btree.root), 6);
		  printf ("\n");
		  fflush (stdout);
		}

	      assert (node);
	      assert (node->region.start == expect);
	    }
	  else
	    {
	      debug (5, "Expected NULL");
	      assert (! node);
	    }
	}
    }

  return 0;
}


