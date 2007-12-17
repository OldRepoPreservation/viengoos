/* Balanced tree insertion and deletion routines.
   Copyright (C) 2004, 2007 Free Software Foundation, Inc.
   Written by Neal H. Walfield <neal@gnu.org>.

   This file is part of the GNU Hurd.

   The GNU Hurd is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2, or (at
   your option) any later version.
   
   The GNU Hurd is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.
   
   You should have received a copy of the GNU General Public License
   along with the GNU Hurd; see the file COPYING.  If not, write to
   the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139,
   USA.  */

/* Copyright (C) 1995, 1996, 1997, 2000 Free Software Foundation, Inc.
   This file is part of the GNU C Library.
   Contributed by Bernd Schmidt <crux@Pool.Informatik.RWTH-Aachen.DE>, 1997.

   The GNU C Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.

   The GNU C Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with the GNU C Library; if not, write to the Free
   Software Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
   02111-1307 USA.  */

/* The insertion and deletion code is taken from the GNU C Library.
   Specifically from the file: misc/tsearch.c.  This code was written
   in 1997 and thus has gotten a fair amount of testing.  In fact, the
   last non-trivial change or bug fix (according to the ChangeLog) was
   this one:

	1997-05-31 02:33  Ulrich Drepper  <drepper@cygnus.com>

	        * misc/tsearch.c: Rewrite tdestroy_recursive.

   So, it looks pretty stable.  */



/* Tree search for red/black trees.
   The algorithm for adding nodes is taken from one of the many "Algorithms"
   books by Robert Sedgewick, although the implementation differs.
   The algorithm for deleting nodes can probably be found in a book named
   "Introduction to Algorithms" by Cormen/Leiserson/Rivest.  At least that's
   the book that my professor took most algorithms from during the "Data
   Structures" course...

   Totally public domain.  */

/* Red/black trees are binary trees in which the edges are colored either red
   or black.  They have the following properties:
   1. The number of black edges on every path from the root to a leaf is
      constant.
   2. No two red edges are adjacent.
   Therefore there is an upper bound on the length of every path, it's
   O(log n) where n is the number of nodes in the tree.  No path can be longer
   than 1+2*P where P is the length of the shortest path in the tree.
   Useful for the implementation:
   3. If one of the children of a node is NULL, then the other one is red
      (if it exists).

   In the implementation, not the edges are colored, but the nodes.  The color
   interpreted as the color of the edge leading to this node.  The color is
   meaningless for the root node, but we color the root node black for
   convenience.  All added nodes are red initially.

   Adding to a red/black tree is rather easy.  The right place is searched
   with a usual binary tree search.  Additionally, whenever a node N is
   reached that has two red successors, the successors are colored black and
   the node itself colored red.  This moves red edges up the tree where they
   pose less of a problem once we get to really insert the new node.  Changing
   N's color to red may violate rule 2, however, so rotations may become
   necessary to restore the invariants.  Adding a new red leaf may violate
   the same rule, so afterwards an additional check is run and the tree
   possibly rotated.

   Deleting is hairy.  There are mainly two nodes involved: the node to be
   deleted (n1), and another node that is to be unchained from the tree (n2).
   If n1 has a successor (the node with a smallest key that is larger than
   n1), then the successor becomes n2 and its contents are copied into n1,
   otherwise n1 becomes n2.
   Unchaining a node may violate rule 1: if n2 is black, one subtree is
   missing one black edge afterwards.  The algorithm must try to move this
   error upwards towards the root, so that the subtree that does not have
   enough black edges becomes the whole tree.  Once that happens, the error
   has disappeared.  It may not be necessary to go all the way up, since it
   is possible that rotations and recoloring can fix the error before that.

   Although the deletion algorithm must walk upwards through the tree, we
   do not store parent pointers in the nodes.  Instead, delete allocates a
   small array of parent pointers and fills it while descending the tree.
   Since we know that the length of a path is O(log n), where n is the number
   of nodes, this is likely to use less memory.  */

/* Tree rotations look like this:
      A                C
     / \              / \
    B   C            A   G
   / \ / \  -->     / \
   D E F G         B   F
                  / \
                 D   E

   In this case, A has been rotated left.  This preserves the ordering of the
   binary tree.  */

#if HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#if 0
#include <search.h>

typedef struct node_t
{
  /* Callers expect this to be the first element in the structure - do not
     move!  */
  const void *key;
  struct node_t *left;
  struct node_t *right;
  unsigned int red:1;
} *node;
typedef const struct node_t *const_node;
#endif

#define BTREE_EXTERN_INLINE 
#include "btree.h"

/* Alias the implementation's type to our public type the former of
   which is a subset of the latter.  */
typedef BTREE_(node_t) *node;

#undef DEBUGGING
#define DEBUGGING
#ifdef DEBUGGING

/* Routines to check tree invariants.  */

#include <assert.h>

#define CHECK_TREE(a) check_tree(a)

static void
check_tree_recurse (node p, BTREE_(key_compare_t) compare, size_t key_offset,
		    int d_sofar, int d_total, bool check_colors)
{
  if (p == NULL)
    {
      if (check_colors)
	assert (d_sofar == d_total);
      return;
    }

  check_tree_recurse (BTREE_NP_CHILD (p->left), compare, key_offset,
		      d_sofar
		      + (BTREE_NP_CHILD (p->left)
			 && !BTREE_NODE_RED_P (BTREE_NP_CHILD (p->left))),
		      d_total, check_colors);
  check_tree_recurse (BTREE_NP_CHILD (p->right), compare, key_offset, 
		      d_sofar
		      + (BTREE_NP_CHILD (p->right)
			 && !BTREE_NODE_RED_P (BTREE_NP_CHILD (p->right))),
		      d_total, check_colors);
  if (BTREE_NP_CHILD (p->left))
    {
      assert (!(BTREE_NODE_RED_P (BTREE_NP_CHILD (p->left))
		&& BTREE_NODE_RED_P (p)));
      assert (BTREE_NP (BTREE_NP_CHILD (p->left)->parent) == p);
      if (compare)
	assert (compare ((void *) BTREE_NP_CHILD (p->left) + key_offset,
			 (void *) p + key_offset) < 0);
    }
  if (BTREE_NP_CHILD (p->right))
    {
      assert (!(BTREE_NODE_RED_P (BTREE_NP_CHILD (p->right))
		&& BTREE_NODE_RED_P (p)));
      assert (BTREE_NP (BTREE_NP_CHILD (p->right)->parent) == p);
      if (compare)
	assert (compare ((void *) BTREE_NP_CHILD (p->right) + key_offset,
			 (void *) p + key_offset) > 0);
    }
  else
    /* If it doesn't have a child, then it better have a right
       thread.  */
    assert (BTREE_NP_THREAD_P (p->right));

  if (BTREE_NP_THREAD_P (p->left))
    {
      node orig_prev = BTREE_NP_THREAD (p->left);
      BTREE_NP_CHILD_SET (&p->left, NULL);
      node prev = BTREE_(prev_hard) (p);
      assert (orig_prev == prev);
      BTREE_NP_THREAD_SET (&p->left, prev);
    }

  if (BTREE_NP_THREAD_P (p->right))
    {
      node orig_next = BTREE_NP_THREAD (p->right);
      BTREE_NP_CHILD_SET (&p->right, NULL);
      node next = BTREE_(next_hard) (p);
      assert (orig_next == next);
      BTREE_NP_THREAD_SET (&p->right, next);
    }
}

void
BTREE_(check_tree_internal) (node root, BTREE_(key_compare_t) compare,
			     size_t key_offset, bool check_colors)
{
  int cnt = 0;
  node p;
  if (root == NULL)
    return;
  BTREE_NODE_RED_SET (root, 0);
  if (check_colors)
    for(p = BTREE_NP_CHILD (root->left); p; p = BTREE_NP_CHILD (p->left))
      cnt += !BTREE_NODE_RED_P (p);
  check_tree_recurse (root, compare, key_offset, 0, cnt, check_colors);
}


#else

#define CHECK_TREE(a)

#endif

/* Return the node following node NODE or NULL if NODE is the last
   (i.e. largest) node in the tree.  The tree needs to be sane,
   however, unlike with BTREE_(next), leaf nodes may have right pointers
   which are NULL.  */
BTREE_(node_t) *
BTREE_(next_hard) (BTREE_(node_t) *node)
{
  if (BTREE_NP_THREAD_P (node->right))
    /* We have a right thread, use it.  */
    return BTREE_NP_THREAD (node->right);

  /* If NODE has a right child node then the left most child of
     NODE->RIGHT is the next node.

                         4
                     2       6
                   1   3   5   7

     The node after 2 is 3, the node after 4 is 5, the node after 6 is
     7. */
  /* NODE->RIGHT must be a link.  */
  if (BTREE_NP_CHILD (node->right))
    {
      node = BTREE_NP_CHILD (node->right);
      while (BTREE_NP_CHILD (node->left))
	node = BTREE_NP_CHILD (node->left);
      return node;
    }

  /* If the node does not have a right child and does not have a
     parent then we either:
                  N
       N   or    /
                O

     In either case, NODE is the last node.  */
  if (! BTREE_NP (node->parent))
    return NULL;

  /* If NODE is the left child node of its parent (and NODE has no
     right nodes) then the parent is the next node.  The node after 1
     is 2, the node after 5 is 6.  */
  if (BTREE_NP_CHILD (BTREE_NP (node->parent)->left) == node)
    return BTREE_NP (node->parent);

  /* If NODE is the right node of its parent (and NODE has no right
     nodes and is not the left not of its parent) then the parent of
     the first ancestor which is a left node of its parent is the next
     node.  The node after 3 is 4.  */
  assert (BTREE_NP_CHILD (BTREE_NP (node->parent)->right) == node);

  while (BTREE_NP (node->parent)
	 && node == BTREE_NP_CHILD (BTREE_NP (node->parent)->right))
    node = BTREE_NP (node->parent);
  return BTREE_NP (node->parent);
}

/* Return the node preceding node NODE or NULL if NODE is the first
   (i.e. smallest) node in the tree.  */
BTREE_(node_t) *
BTREE_(prev_hard) (BTREE_(node_t) *node)
{
  if (BTREE_NP_THREAD_P (node->left))
    /* We have a left thread, use it.  */
    return BTREE_NP_THREAD (node->left);

  /* If NODE has a left child node then the right most child of it
     (NODE->RIGHT) is the previous node.

                         4
                     2       6
                   1   3   5   7

     The node before 2 is 1, 4 is 3, 6 is 5. */
  if (BTREE_NP_CHILD (node->left))
    {
      node = BTREE_NP_CHILD (node->left);
      while (BTREE_NP_CHILD (node->right))
	node = BTREE_NP_CHILD (node->right);
      return node;
    }

  /* If the node does not have a left child and does not have a
     parent then we either:
                  N
       N   or      \
                    O

     In either case, NODE is the first node.  */
  if (! BTREE_NP (node->parent))
    return NULL;

  /* If NODE is the right child node of its parent (and NODE has no
     right nodes) then the parent is the next node.  The node before 3
     is 2, the node before 7 is 6.  */
  if (BTREE_NP_CHILD (BTREE_NP (node->parent)->right) == node)
    return BTREE_NP (node->parent);

  /* If NODE is the left node of its parent (and NODE has no left
     nodes and is not the right not of its parent) then the parent of
     the first ancestor which is a right node of its parent is the
     next node.  The node before 3 is 4.  */
  assert (BTREE_NP_CHILD (BTREE_NP (node->parent)->left) == node);

  while (BTREE_NP (node->parent)
	 && node == BTREE_NP_CHILD (BTREE_NP (node->parent)->left))
    node = BTREE_NP (node->parent);
  return BTREE_NP (node->parent);
}

/* Return the location of the NODE's parent's child pointer that
   designates NODE, i.e., either &node->parent->left,
   &node->parent->right, or &root.  */
static struct BTREE_(node_ptr) *
selfp (BTREE_(t) *btree, BTREE_(node_t) *node)
{
  if (! BTREE_NP (node->parent))
    {
      assert (BTREE_NP (btree->root) == node);
      return &btree->root;
    }
  else if (BTREE_NP_CHILD (BTREE_NP (node->parent)->left) == node)
    return &BTREE_NP (node->parent)->left;
  else
    {
      assert (BTREE_NP_CHILD (BTREE_NP (node->parent)->right) == node);
      return &BTREE_NP (node->parent)->right;
    }
}

/* Possibly "split" a node with two red successors, and/or fix up two red
   edges in a row.  ROOTP is a pointer to the lowest node we visited, PARENTP
   and GPARENTP pointers to its parent/grandparent.  P_R and GP_R contain the
   comparison values that determined which way was taken in the tree to reach
   ROOTP.  MODE is 0 if we need not do the split, but must check for two red
   edges between GPARENTP and ROOTP.  */
void
BTREE_(maybe_split2_internal) (BTREE_(t) *btree, node root, int mode)
{
  struct BTREE_(node_ptr) *rp, *lp;
  rp = &root->right;
  lp = &root->left;

  /* See if we have to split this node (both successors red).  */
  if (mode == 1
      || (BTREE_NP_CHILD (*rp) && BTREE_NP_CHILD (*lp)
	  && BTREE_NODE_RED_P (BTREE_NP_CHILD (*rp))
	  && BTREE_NODE_RED_P (BTREE_NP_CHILD (*lp))))
    {
      /* This node becomes red, its successors black.  */
      BTREE_NODE_RED_SET (root, 1);
      if (BTREE_NP_CHILD (*rp))
	BTREE_NODE_RED_SET (BTREE_NP_CHILD (*rp), 0);
      if (BTREE_NP_CHILD (*lp))
	BTREE_NODE_RED_SET (BTREE_NP_CHILD (*lp), 0);

      /* If the parent of this node is also red, we have to do
	 rotations.  */
      if (BTREE_NP (root->parent)
	  && BTREE_NODE_RED_P (BTREE_NP (root->parent)))
	{
	  node gp, p;
	  struct BTREE_(node_ptr) *parentp, *gparentp;
	  int p_r, gp_r;

	  p = BTREE_NP (root->parent);
	  /* P cannot be the root of the tree: it is red and the root
	     is black.  */
	  assert (BTREE_NP (p->parent));

	  /* Avoid a branch.  -1 is left, 1 is right.  */
	  p_r = (BTREE_NP_CHILD (p->right) == root) * 2 - 1;

	  if (BTREE_NP_CHILD (BTREE_NP (p->parent)->left) == p)
	    {
	      parentp = &BTREE_NP (p->parent)->left;
	      gp_r = -1;
	    }
	  else
	    {
	      assert (BTREE_NP_CHILD (BTREE_NP (p->parent)->right) == p);
	      parentp = &BTREE_NP (p->parent)->right;
	      gp_r = 1;
	    }

	  /* P must have a parent since it cannot be the root.  */
	  gp = BTREE_NP (p->parent);
	  assert (gp);
	  gparentp = selfp (btree, gp);

	  /* There are two main cases:
	     1. The edge types (left or right) of the two red edges differ.
	     2. Both red edges are of the same type.
	     There exist two symmetries of each case, so there is a total of
	     4 cases.  */
	  if ((p_r > 0) != (gp_r > 0))
	    {
	      /* Put the child at the top of the tree, with its parent
		 and grandparent as successors.  */
	      BTREE_NODE_RED_SET (p, 1);
	      BTREE_NODE_RED_SET (gp, 1);
	      BTREE_NODE_RED_SET (root, 0);

	      BTREE_NP_CHILD_SET (gparentp, root);
	      BTREE_NP_SET (&root->parent, BTREE_NP (gp->parent));

	      if (p_r < 0)
		{
		  /* Child is left of parent.
		      |           |
                      g          root
                       \        /    \
                        p  ->  g      p
                       /        \    /
                     root        l  r
                     /  \
                    l    r */
		  BTREE_NP_CHILD_SET (&p->left, BTREE_NP_CHILD (*rp));
		  if (BTREE_NP_CHILD (p->left))
		    BTREE_NP_SET (&BTREE_NP_CHILD (p->left)->parent, p);

		  BTREE_NP_CHILD_SET (rp, p);
		  BTREE_NP_SET (&p->parent, root);

		  BTREE_NP_CHILD_SET (&gp->right, BTREE_NP_CHILD (*lp));
		  if (BTREE_NP_CHILD (gp->right))
		    BTREE_NP_SET (&BTREE_NP_CHILD (gp->right)->parent, gp);

		  BTREE_NP_CHILD_SET (lp, gp);
		  BTREE_NP_SET (&gp->parent, root);

		  if (! BTREE_NP_CHILD (gp->right))
		    BTREE_NP_THREAD_SET (&gp->right, BTREE_(next_hard) (gp));
		}
	      else
		{
		  /* Child is right of parent.
		      |           |
                      g          root
                     /          /    \
                    p      ->  p      gp
                     \          \    /
                     root        l  r
                     /  \
                    l    r */
		  BTREE_NP_CHILD_SET (&p->right, BTREE_NP_CHILD (*lp));
		  if (BTREE_NP_CHILD (p->right))
		    BTREE_NP_SET (&BTREE_NP_CHILD (p->right)->parent, p);

		  BTREE_NP_CHILD_SET (lp, p);
		  BTREE_NP_SET (&p->parent, root);

		  BTREE_NP_CHILD_SET (&gp->left, BTREE_NP_CHILD (*rp));
		  if (BTREE_NP_CHILD (gp->left))
		    BTREE_NP_SET (&BTREE_NP_CHILD (gp->left)->parent, gp);

		  BTREE_NP_CHILD_SET (rp, gp);
		  BTREE_NP_SET (&gp->parent, root);

		  if (! BTREE_NP_CHILD (p->right))
		    BTREE_NP_THREAD_SET (&p->right, BTREE_(next_hard) (p));
		}
	    }
	  else
	    {
	      /* Parent becomes the top of the tree, grandparent and
		 child are its successors.  */
	      BTREE_NODE_RED_SET (p, 0);
	      BTREE_NODE_RED_SET (gp, 1);

	      BTREE_NP_CHILD_SET (gparentp, p);
	      BTREE_NP_SET (&p->parent, BTREE_NP (gp->parent));

	      if (p_r < 0)
		{
		  /* Left edges.
		         |          |
                         g          p
                        /          / \
                       p   ->  root    g
                      / \      /  \   /
                   root  pr          pr
                   /  \
		   */
		  BTREE_NP_CHILD_SET (&gp->left, BTREE_NP_CHILD (p->right));
		  if (BTREE_NP_CHILD (gp->left))
		    BTREE_NP_SET (&BTREE_NP_CHILD (gp->left)->parent, gp);
		  BTREE_NP_CHILD_SET (&p->right, gp);
		  BTREE_NP_SET (&gp->parent, p);
		}
	      else
		{
		  /* Right edges.
		      |           |
                      g           p
                       \        /   \
                        p  ->  g     root
                       / \      \    /  \
                     pl  root    pl
                         /  \
		   */
		  BTREE_NP_CHILD_SET (&gp->right, BTREE_NP_CHILD (p->left));
		  if (BTREE_NP_CHILD (gp->right))
		    BTREE_NP_SET (&BTREE_NP_CHILD (gp->right)->parent, gp);
		  BTREE_NP_CHILD_SET (&p->left, gp);
		  BTREE_NP_SET (&gp->parent, p);

		  if (! BTREE_NP_CHILD (gp->right))
		    BTREE_NP_THREAD_SET (&gp->right, BTREE_(next_hard) (gp));
		}
	    }
	}

      /* We have changed the color of the root of the tree to red.
	 Making the root node black never changes the black height of
	 the tree, however, it does eliminate a bit of work when both
	 its children are red.  */
      BTREE_NODE_RED_SET (BTREE_NP (btree->root), 0);
    }
}

#if 0

/* Find or insert datum into search tree.
   KEY is the key to be located, ROOTP is the address of tree root,
   COMPAR the ordering function.  */
void *
__tsearch (const void *key, void **vrootp, __compar_fn_t compar)
{
  node q;
  node *parentp = NULL, *gparentp = NULL;
  node *rootp = (node *) vrootp;
  node *nextp;
  int r = 0, p_r = 0, gp_r = 0; /* No they might not, Mr Compiler.  */

  if (rootp == NULL)
    return NULL;

  /* This saves some additional tests below.  */
  if (*rootp != NULL)
    (*rootp)->red = 0;

  CHECK_TREE (*rootp);

  nextp = rootp;
  while (*nextp != NULL)
    {
      node root = *rootp;
      r = (*compar) (key, root->key);
      if (r == 0)
	return root;

      maybe_split_for_insert (rootp, parentp, gparentp, p_r, gp_r, 0);
      /* If that did any rotations, parentp and gparentp are now garbage.
	 That doesn't matter, because the values they contain are never
	 used again in that case.  */

      nextp = r < 0 ? &root->left : &root->right;
      if (*nextp == NULL)
	break;

      gparentp = parentp;
      parentp = rootp;
      rootp = nextp;

      gp_r = p_r;
      p_r = r;
    }

  q = (struct node_t *) malloc (sizeof (struct node_t));
  if (q != NULL)
    {
      *nextp = q;			/* link new node to old */
      q->key = key;			/* initialize new node */
      q->red = 1;
      q->left = q->right = NULL;
    }
  if (nextp != rootp)
    /* There may be two red edges in a row now, which we must avoid by
       rotating the tree.  */
    maybe_split_for_insert (nextp, rootp, parentp, r, p_r, 1);

  return q;
}
weak_alias (__tsearch, tsearch)


/* Find datum in search tree.
   KEY is the key to be located, ROOTP is the address of tree root,
   COMPAR the ordering function.  */
void *
__tfind (key, vrootp, compar)
     const void *key;
     void *const *vrootp;
     __compar_fn_t compar;
{
  node *rootp = (node *) vrootp;

  if (rootp == NULL)
    return NULL;

  CHECK_TREE (*rootp);

  while (*rootp != NULL)
    {
      node root = *rootp;
      int r;

      r = (*compar) (key, root->key);
      if (r == 0)
	return root;

      rootp = r < 0 ? &root->left : &root->right;
    }
  return NULL;
}
weak_alias (__tfind, tfind)


/* Delete node with given key.
   KEY is the key to be deleted, ROOTP is the address of the root of tree,
   COMPAR the comparison function.  */
void *
__tdelete (const void *key, void **vrootp, __compar_fn_t compar)
{
  node p, q, r, retval;
  int cmp;
  node *rootp = (node *) vrootp;
  node root, unchained;
  /* Stack of nodes so we remember the parents without recursion.  It's
     _very_ unlikely that there are paths longer than 40 nodes.  The tree
     would need to have around 250.000 nodes.  */
  int stacksize = 40;
  int sp = 0;
  node **nodestack = alloca (sizeof (node *) * stacksize);

  if (rootp == NULL)
    return NULL;
  p = *rootp;
  if (p == NULL)
    return NULL;

  CHECK_TREE (p);

  while ((cmp = (*compar) (key, (*rootp)->key)) != 0)
    {
      if (sp == stacksize)
	{
	  node **newstack;
	  stacksize += 20;
	  newstack = alloca (sizeof (node *) * stacksize);
	  nodestack = memcpy (newstack, nodestack, sp * sizeof (node *));
	}

      nodestack[sp++] = rootp;
      p = *rootp;
      rootp = ((cmp < 0)
	       ? &(*rootp)->left
	       : &(*rootp)->right);
      if (*rootp == NULL)
	return NULL;
    }

  /* This is bogus if the node to be deleted is the root... this routine
     really should return an integer with 0 for success, -1 for failure
     and errno = ESRCH or something.  */
  retval = p;
  ...
}
#endif

/* Detach node ROOT from the tree BTREE.  */
void
BTREE_(detach) (BTREE_(t) *btree, BTREE_(node_t) *root)
{
  node p, q, child;
  struct BTREE_(node_ptr) *rootp;
  node unchained;
  int unchained_is_red;

  /* We don't unchain the node we want to delete. Instead, we overwrite
     it with its successor and unchain the successor.  If there is no
     successor, we really unchain the node to be deleted.

                   8
                 /  \
               3     ...
	     /   \
	    1     6
	   / \   / \
	  0   2 4   7
                 \
                  5

     Assume we want to delete a node with 2 children, e.g. ROOT=3.  It
     is trivial to attach either the left or right branch to where 3
     was but what happens to the other branch?  Well the successor
     will always be a node with less than two children.  Replacing the
     node we want to delete with the successor maintains the ordering
     and means we just have to deal with its successor (the trivial
     case--we just move it up).

     In this example, the successor, UNCHAINED, would be 4 which has
     less than two children.  The child, CHILD, is node 5.  We unchain 4
     and insert R where it was.  Then we replace ROOT with UNCHAINED.
     The resulting tree is thus:
     
                   8
                 /  \
               4     ...
	     /   \
	    1     6
	   / \   / \
	  0   2 5   7

     Assume that we want to remove a node with one child, for instance,
     ROOT=4 (from the first figure).  This is trivially detached as it
     need merely be removed from the tree and its one child moved up
     to take its place.  The resulting tree would be:

                   8
                 /  \
               3     ...
	     /   \
	    1     6
	   / \   / \
	  0   2 5   7

     Once this is done, we may need to recolor the tree.
   */

  rootp = selfp (btree, root);

  child = BTREE_NP_CHILD (root->right);
  q = BTREE_NP_CHILD (root->left);

  if (q == NULL || child == NULL)
    unchained = root;
  else
    {
      unchained = BTREE_(next) (root);
      assert (!BTREE_NP_CHILD (unchained->left)
	      || !BTREE_NP_CHILD (unchained->right));
    }

  /* We know that at least either, but perhaps both, of the left or
     right children of UNCHAINED is NULL.  CHILD becomes the other one
     and is chained into the parent of UNCHAINED.  */
  child = BTREE_NP_CHILD (unchained->left);
  if (child == NULL)
    child = BTREE_NP_CHILD (unchained->right);
  else
    assert (! BTREE_NP_CHILD (unchained->right));

  /* Update ROOT's predecessor's and successor's threads.  This is
     required to avoid dangling pointers.  */
  node pred = BTREE_(prev) (root);
  node succ;
  if (unchained != root)
    /* UNCHAINED is the successor.  */
    succ = unchained;
  else
    succ = BTREE_(next) (root);

  if (pred && BTREE_NP_THREAD_P (pred->right))
    BTREE_NP_THREAD_SET (&pred->right, succ);
  if (succ && BTREE_NP_THREAD_P (succ->left))
    BTREE_NP_THREAD_SET (&succ->left, pred);

  /* Cache CHILD's parent as we may not be able to recover it if CHILD is
     NULL and UNCHAINED is clobbered.  */
  if (BTREE_NP (unchained->parent) == root)
    p = unchained;
  else
    p = BTREE_NP (unchained->parent);

  /* And get UNCHAINED's parent to point to CHILD.  */
  if (! BTREE_NP (unchained->parent))
    {
      /* UNCHAINED is only ever the root when either there is one or
	 two nodes in the tree.  */
      assert (unchained == root);
      BTREE_NP_SET (&btree->root, child);
    }
  else if (BTREE_NP_CHILD (BTREE_NP (unchained->parent)->left) == unchained)
    {
      /* UNCHAINED can't be to the immediate left of ROOT as it is
	 ROOT's successor.  */
      assert (BTREE_NP (unchained->parent) != root);
      BTREE_NP_CHILD_SET (&BTREE_NP (unchained->parent)->left, child);
    }
  else
    {
      assert (BTREE_NP_CHILD (BTREE_NP (unchained->parent)->right)
	      == unchained);

      if (child)
	BTREE_NP_SET (&BTREE_NP (unchained->parent)->right, child);
      else
	/* CHILD is NULL and this is a right leaf.  */
	BTREE_NP_THREAD_SET (&BTREE_NP (unchained->parent)->right,
			     BTREE_(next_hard) (unchained));
    }

  /* Adjust CHILD's parent pointer (we could't do this earlier as the call to
     BTREE_(next_hard) would fail because we wouldn't have a proper
     tree).  */
  if (child)
    BTREE_NP_SET (&child->parent, p);

  unchained_is_red = BTREE_NODE_RED_P (unchained);

  /* Replace the element to detach with its now unchained
     successor.  */
  if (unchained != root)
    {
      BTREE_NP_CHILD_SET (&unchained->left, BTREE_NP_CHILD (root->left));
      if (BTREE_NP_CHILD (unchained->left))
	BTREE_NP_SET (&BTREE_NP (unchained->left)->parent, unchained);

      /* If ROOT->RIGHT is a right thread then it remains correct.  */
      if (BTREE_NP_CHILD (root->right))
	{
	  BTREE_NP_CHILD_SET (&unchained->right, BTREE_NP_CHILD (root->right));
	  if (BTREE_NP_CHILD (unchained->right))
	    BTREE_NP_SET (&BTREE_NP_CHILD (unchained->right)->parent,
			  unchained);
	}

      BTREE_NP_SET (&unchained->parent, BTREE_NP (root->parent));
      BTREE_NODE_RED_SET (unchained, BTREE_NODE_RED_P (root));

      BTREE_NP_CHILD_SET (rootp, unchained);
    }

  if (!unchained_is_red)
    {
      /* Now we lost a black edge, which means that the number of black
	 edges on every path is no longer constant.  We must balance the
	 tree.  */
      /* P is the parent of CHILD (now that UNCHAINED has been detached).
	 CHILD is likely to be NULL in the first iteration.  */
      /* NULL nodes are considered black throughout - this is necessary for
	 correctness.  */
      for (; p && (child == NULL || !BTREE_NODE_RED_P (child));
	   p = BTREE_NP (p->parent))
	{
	  BTREE_ (check_tree_internal) (BTREE_NP (btree->root), 0, 0, false);

	  struct BTREE_(node_ptr) *pp = selfp (btree, p);

	  assert (! child || BTREE_NP (child->parent) == p);

	  /* Two symmetric cases.  */
	  if (child == BTREE_NP_CHILD (p->left))
	    {
	      /* Q is CHILD's brother, P is CHILD's parent.  The
		 subtree with root CHILD has one black edge less than
		 the subtree with root Q.

		             p
			    / \
		       child   q
	       */
	      q = BTREE_NP_CHILD (p->right);
	      assert (q);
	      if (BTREE_NODE_RED_P (q))
		{
		  /* If Q is red, we know that P is black. We rotate P left
		     so that Q becomes the top node in the tree, with P below
		     it.  P is colored red, Q is colored black.
		     This action does not change the black edge count for any
		     leaf in the tree, but we will be able to recognize one
		     of the following situations, which all require that Q
		     is black.

		                q
		               / \
		      ->      p   pr
			     / \
		        child   ql

		   */
		  BTREE_NODE_RED_SET (q, 0);
		  BTREE_NODE_RED_SET (p, 1);
		  /* Left rotate p.  */
		  BTREE_NP_SET (&q->parent, BTREE_NP (p->parent));
		  BTREE_NP_CHILD_SET (&p->right, BTREE_NP_CHILD (q->left));
		  if (BTREE_NP_CHILD (p->right))
		    BTREE_NP_SET (&BTREE_NP_CHILD (p->right)->parent, p);

		  BTREE_NP_CHILD_SET (&q->left, p);
		  BTREE_NP_SET (&p->parent, q);
		  
		  BTREE_NP_CHILD_SET (pp, q);
		  /* Make sure pp is right if the case below tries to use
		     it.  */
		  pp = &q->left;
		  q = BTREE_NP_CHILD (p->right);
		  assert (q);

		  if (! BTREE_NP_CHILD (p->right))
		    BTREE_NP_THREAD_SET (&p->right, BTREE_(next_hard) (p));
		}
	      /* We know that Q can't be NULL here.  We also know that Q is
		 black.  */
	      assert (! BTREE_NODE_RED_P (q));

	      if ((BTREE_NP_CHILD (q->left) == NULL
		   || !BTREE_NODE_RED_P (BTREE_NP_CHILD (q->left)))
		  && (BTREE_NP_CHILD (q->right) == NULL
		      || !BTREE_NODE_RED_P (BTREE_NP_CHILD (q->right))))
		{
		  /* Q has two black successors.  We can simply color Q red.
		     The whole subtree with root P is now missing one black
		     edge.  Note that this action can temporarily make the
		     tree invalid (if P is red).  But we will exit the loop
		     in that case and set P black, which both makes the tree
		     valid and also makes the black edge count come out
		     right.  If P is black, we are at least one step closer
		     to the root and we'll try again the next iteration.  */
		  BTREE_NODE_RED_SET (q, 1);
		  child = p;
		}
	      else
		{
		  /* Q is black, one of Q's successors is red.  We can
		     repair the tree with one operation and will exit the
		     loop afterwards.  */
		  if (! BTREE_NP_CHILD (q->right)
		      || !BTREE_NODE_RED_P (BTREE_NP_CHILD (q->right)))
		    {
		      /* The left one is red.  We perform the same
			 action as in maybe_split_for_insert where two
			 red edges are adjacent but point in different
			 directions: Q's left successor (let's call it
			 Q2) becomes the top of the subtree we are
			 looking at, its parent (Q) and grandparent
			 (P) become its successors. The former
			 successors of Q2 are placed below P and Q.  P
			 becomes black, and Q2 gets the color that P
			 had.  This changes the black edge count only
			 for node CHILD and its successors.

		             p                q2
			    / \             /    \
		       child   q    ->     p      q
                              / \         / \    / \
                             q2      child  q2l
		      */
		      node q2 = BTREE_NP_CHILD (q->left);
		      BTREE_NODE_RED_SET (q2, BTREE_NODE_RED_P (p));

		      BTREE_NP_SET (&q2->parent, BTREE_NP (p->parent));

		      BTREE_NP_CHILD_SET (&p->right, BTREE_NP_CHILD (q2->left));
		      if (BTREE_NP_CHILD (p->right))
			BTREE_NP_SET (&BTREE_NP_CHILD (p->right)->parent, p);

		      BTREE_NP_CHILD_SET (&q->left, BTREE_NP_CHILD (q2->right));
		      if (BTREE_NP_CHILD (q->left))
			BTREE_NP_SET (&BTREE_NP_CHILD (q->left)->parent, q);

		      BTREE_NP_CHILD_SET (&q2->right, q);
		      BTREE_NP_SET (&q->parent, q2);

		      BTREE_NP_CHILD_SET (&q2->left, p);
		      BTREE_NP_SET (&p->parent, q2);

		      BTREE_NP_CHILD_SET (pp, q2);
		      BTREE_NODE_RED_SET (p, 0);

		      if (! BTREE_NP_CHILD (p->right))
			BTREE_NP_THREAD_SET (&p->right, BTREE_(next_hard) (p));
		    }
		  else
		    {
		      /* It's the right one.  Rotate P left. P becomes black,
			 and Q gets the color that P had.  Q's right successor
			 also becomes black.  This changes the black edge
			 count only for node CHILD and its successors.

		                q
		               / \
		      ->      p   pr
			     / \
			child   ql
		       */

		      BTREE_NODE_RED_SET (q, BTREE_NODE_RED_P (p));
		      BTREE_NODE_RED_SET (p, 0);

		      BTREE_NP_SET (&q->parent, BTREE_NP (p->parent));

		      BTREE_NODE_RED_SET (BTREE_NP_CHILD (q->right), 0);

		      /* left rotate p */
		      BTREE_NP_CHILD_SET (&p->right, BTREE_NP_CHILD (q->left));
		      if (BTREE_NP_CHILD (p->right))
			BTREE_NP_SET (&BTREE_NP_CHILD (p->right)->parent, p);

		      BTREE_NP_CHILD_SET (&q->left, p);
		      BTREE_NP_SET (&p->parent, q);

		      BTREE_NP_CHILD_SET (pp, q);

		      if (! BTREE_NP_CHILD (p->right))
			BTREE_NP_THREAD_SET (&p->right, BTREE_(next_hard) (p));
		    }

		  /* We're done.  */
		  return;
		}
	    }
	  else
	    {
	      assert (child == BTREE_NP_CHILD (p->right));
	      /* Comments: see above.  */
	      q = BTREE_NP_CHILD (p->left);
	      if (q != NULL && BTREE_NODE_RED_P (q))
		{
		  BTREE_NODE_RED_SET (q, 0);
		  BTREE_NODE_RED_SET (p, 1);
		  BTREE_NP_SET (&q->parent, BTREE_NP (p->parent));
		  BTREE_NP_CHILD_SET (&p->left, BTREE_NP_CHILD (q->right));
		  if (BTREE_NP_CHILD (p->left))
		    BTREE_NP_SET (&BTREE_NP_CHILD (p->left)->parent, p);
		  BTREE_NP_CHILD_SET (&q->right, p);
		  BTREE_NP_SET (&p->parent, q);
		  BTREE_NP_CHILD_SET (pp, q);
		  pp = &q->right;
		  q = BTREE_NP_CHILD (p->left);
		}
	      assert (! BTREE_NODE_RED_P (q));
	      if ((! BTREE_NP_CHILD (q->right)
		   || ! BTREE_NODE_RED_P (BTREE_NP_CHILD (q->right)))
		  && (BTREE_NP_CHILD (q->left) == NULL
		      || ! BTREE_NODE_RED_P (BTREE_NP_CHILD (q->left))))
		{
		  BTREE_NODE_RED_SET (q, 1);
		  child = p;
		}
	      else
		{
		  if (!BTREE_NP_CHILD (q->left)
		      || !BTREE_NODE_RED_P (BTREE_NP_CHILD (q->left)))
		    {
		      node q2 = BTREE_NP_CHILD (q->right);
		      BTREE_NODE_RED_SET (q2, BTREE_NODE_RED_P (p));
		      BTREE_NP_SET (&q2->parent, BTREE_NP (p->parent));
		      BTREE_NP_CHILD_SET (&p->left, BTREE_NP_CHILD (q2->right));
		      if (BTREE_NP_CHILD (p->left))
			BTREE_NP_SET (&BTREE_NP_CHILD (p->left)->parent, p);
		      BTREE_NP_CHILD_SET (&q->right, BTREE_NP_CHILD (q2->left));
		      if (BTREE_NP_CHILD (q->right))
			BTREE_NP_SET (&BTREE_NP_CHILD (q->right)->parent, q);
		      BTREE_NP_CHILD_SET (&q2->left, q);
		      BTREE_NP_SET (&q->parent, q2);
		      BTREE_NP_CHILD_SET (&q2->right, p);
		      BTREE_NP_SET (&p->parent, q2);
		      BTREE_NP_CHILD_SET (pp, q2);
		      BTREE_NODE_RED_SET (p, 0);

		      if (! BTREE_NP_CHILD (q->right))
			BTREE_NP_THREAD_SET (&q->right, BTREE_(next_hard) (q));
		    }
		  else
		    {
		      BTREE_NODE_RED_SET (q, BTREE_NODE_RED_P (p));
		      BTREE_NODE_RED_SET (p, 0);
		      BTREE_NODE_RED_SET (BTREE_NP_CHILD (q->left), 0);
		      BTREE_NP_SET (&q->parent, BTREE_NP (p->parent));
		      BTREE_NP_CHILD_SET (&p->left, BTREE_NP_CHILD (q->right));
		      if (BTREE_NP_CHILD (p->left))
			BTREE_NP_SET (&BTREE_NP_CHILD (p->left)->parent, p);
		      BTREE_NP_CHILD_SET (&q->right, p);
		      BTREE_NP_SET (&p->parent, q);
		      BTREE_NP_CHILD_SET (pp, q);
		    }
		  return;
		}
	    }
	}
      if (child != NULL)
	BTREE_NODE_RED_SET (child, 0);
    }
}

#if 0
{
  ...
  free (unchained);
  return retval;
}
weak_alias (__tdelete, tdelete)


/* Walk the nodes of a tree.
   ROOT is the root of the tree to be walked, ACTION the function to be
   called at each node.  LEVEL is the level of ROOT in the whole tree.  */
static void
internal_function
trecurse (const void *vroot, __action_fn_t action, int level)
{
  const_node root = (const_node) vroot;

  if (root->left == NULL && root->right == NULL)
    (*action) (root, leaf, level);
  else
    {
      (*action) (root, preorder, level);
      if (root->left != NULL)
	trecurse (root->left, action, level + 1);
      (*action) (root, postorder, level);
      if (root->right != NULL)
	trecurse (root->right, action, level + 1);
      (*action) (root, endorder, level);
    }
}


/* Walk the nodes of a tree.
   ROOT is the root of the tree to be walked, ACTION the function to be
   called at each node.  */
void
__twalk (const void *vroot, __action_fn_t action)
{
  const_node root = (const_node) vroot;

  CHECK_TREE (root);

  if (root != NULL && action != NULL)
    trecurse (root, action, 0);
}
weak_alias (__twalk, twalk)



/* The standardized functions miss an important functionality: the
   tree cannot be removed easily.  We provide a function to do this.  */
static void
internal_function
tdestroy_recurse (node root, __free_fn_t freefct)
{
  if (root->left != NULL)
    tdestroy_recurse (root->left, freefct);
  if (root->right != NULL)
    tdestroy_recurse (root->right, freefct);
  (*freefct) ((void *) root->key);
  /* Free the node itself.  */
  free (root);
}

void
__tdestroy (void *vroot, __free_fn_t freefct)
{
  node root = (node) vroot;

  CHECK_TREE (root);

  if (root != NULL)
    tdestroy_recurse (root, freefct);
}
weak_alias (__tdestroy, tdestroy)


#endif
