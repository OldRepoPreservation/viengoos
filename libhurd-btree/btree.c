/* Balanced tree insertion and deletion routines.
   Copyright (C) 2004 Free Software Foundation, Inc.
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
// #define DEBUGGING
#ifdef DEBUGGING

/* Routines to check tree invariants.  */

#include <assert.h>

#define CHECK_TREE(a) check_tree(a)

static void
check_tree_recurse (node p, BTREE_(key_compare_t) compare, size_t key_offset,
		    int d_sofar, int d_total)
{
  if (p == NULL)
    {
      assert (d_sofar == d_total);
      return;
    }

  check_tree_recurse (p->left, compare, key_offset,
		      d_sofar + (p->left && !p->left->red), d_total);
  check_tree_recurse (BTREE_(link_internal) (p->right), compare, key_offset, 
		      d_sofar + (BTREE_(link_internal) (p->right)
				 && !p->right->red), d_total);
  if (p->left)
    {
      assert (!(p->left->red && p->red));
      assert (p->left->parent == p);
      assert (compare ((void *) p->left + key_offset,
		       (void *) p + key_offset) < 0);
    }
  if (BTREE_(link_internal) (p->right))
    {
      assert (!(p->right->red && p->red));
      assert (p->right->parent == p);
      assert (compare ((void *) p->right + key_offset,
		       (void *) p + key_offset) > 0);
    }
}

void
BTREE_(check_tree_internal) (node root, BTREE_(key_compare_t) compare,
			     size_t key_offset)
{
  int cnt = 0;
  node p;
  if (root == NULL)
    return;
  root->red = 0;
  for(p = root->left; p; p = p->left)
    cnt += !p->red;
  check_tree_recurse (root, compare, key_offset, 0, cnt);
}


#else

#define CHECK_TREE(a)

#endif

/* Return the node following node NODE or NULL if NODE is the last
   (i.e. largest) node in the tree.  The tree needs to be sane,
   however, unlike with BTREE_(next), leaf nodes may have right pointers
   which are NULL.  */
static BTREE_(node_t) *
BTREE_(next_hard) (BTREE_(node_t) *node)
{
  if (((uintptr_t) node->right & 1) == 1)
    /* We have a right thread, use it.  */
    return (BTREE_(node_t) *) (((uintptr_t) node->right) & ~1);

  /* If NODE has a right child node then the left most child of
     NODE->RIGHT is the next node.

                         4
                     2       6
                   1   3   5   7

     The node after 2 is 3, the node after 4 is 5, the node after 6 is
     7. */
  /* NODE->RIGHT must be a link.  */
  if (node->right)
    {
      node = node->right;
      while (node->left)
	node = node->left;
      return node;
    }

  /* If the node does not have a right child and does not have a
     parent then we either:
                  N
       N   or    /
                O

     In either case, NODE is the last node.  */
  if (! node->parent)
    return NULL;

  /* If NODE is the left child node of its parent (and NODE has no
     right nodes) then the parent is the next node.  The node after 1
     is 2, the node after 5 is 6.  */
  if (node->parent->left == node)
    return node->parent;

  /* If NODE is the right node of its parent (and NODE has no right
     nodes and is not the left not of its parent) then the parent of
     the first ancestor which is a left node of its parent is the next
     node.  The node after 3 is 4.  */
  assert (node->parent->right == node);

  while (node->parent && node == node->parent->right)
    node = node->parent;
  return node->parent;
}

/* Return the node preceding node NODE or NULL if NODE is the first
   (i.e. smallest) node in the tree.  */
BTREE_(node_t) *
BTREE_(prev) (BTREE_(node_t) *node)
{
  /* If NODE has a left child node then the right most child of it
     (NODE->RIGHT) is the previous node.

                         4
                     2       6
                   1   3   5   7

     The node before 2 is 1, 4 is 3, 6 is 5. */
  if (node->left)
    {
      node = node->left;
      while (BTREE_(link_internal) (node->right))
	node = BTREE_(link_internal) (node->right);
      return node;
    }

  /* If the node does not have a left child and does not have a
     parent then we either:
                  N
       N   or      \
                    O

     In either case, NODE is the first node.  */
  if (! node->parent)
    return NULL;

  /* If NODE is the right child node of its parent (and NODE has no
     right nodes) then the parent is the next node.  The node before 3
     is 2, the node before 7 is 6.  */
  if (node->parent->right == node)
    return node->parent;

  /* If NODE is the left node of its parent (and NODE has no left
     nodes and is not the right not of its parent) then the parent of
     the first ancestor which is a right node of its parent is the
     next node.  The node before 3 is 4.  */
  assert (node->parent->left == node);

  while (node->parent && node == node->parent->left)
    node = node->parent;
  return node->parent;
}

static BTREE_(node_t) **
selfp (BTREE_(t) *btree, BTREE_(node_t) *node)
{
  assert (((uintptr_t) node & 1) == 0);

  if (! node->parent)
    {
      assert (btree->root == node);
      return &btree->root;
    }
  else if (node->parent->left == node)
    return &node->parent->left;
  else
    {
      assert (node->parent->right == node);
      return &node->parent->right;
    }
}

/* Possibly "split" a node with two red successors, and/or fix up two red
   edges in a row.  ROOTP is a pointer to the lowest node we visited, PARENTP
   and GPARENTP pointers to its parent/grandparent.  P_R and GP_R contain the
   comparison values that determined which way was taken in the tree to reach
   ROOTP.  MODE is 1 if we need not do the split, but must check for two red
   edges between GPARENTP and ROOTP.  */
void
BTREE_(maybe_split2_internal) (BTREE_(t) *btree, node root, int mode)
{
  node *rp, *lp;
  rp = &root->right;
  lp = &root->left;

  /* Make sure we didn't get a right thread.  */
  assert (((uintptr_t) root & 1) == 0);

  /* See if we have to split this node (both successors red).  */
  if (mode == 1
      || (BTREE_(link_internal) (*rp) && (*lp) && (*rp)->red && (*lp)->red))
    {
      /* This node becomes red, its successors black.  */
      root->red = 1;
      if (BTREE_(link_internal) (*rp))
	(*rp)->red = 0;
      if (*lp)
	(*lp)->red = 0;

      /* If the parent of this node is also red, we have to do
	 rotations.  */
      if (root->parent && root->parent->red)
	{
	  node gp, p;
	  node *parentp, *gparentp;
	  int p_r, gp_r;

	  p = root->parent;
	  /* P cannot be the root of the tree: it is red and the root
	     is black.  */
	  assert (p->parent);

	  /* Avoid a branch.  -1 is left, 1 is right.  */
	  p_r = (p->right == root) * 2 - 1;

	  if (p->parent->left == p)
	    {
	      parentp = &p->parent->left;
	      gp_r = -1;
	    }
	  else
	    {
	      assert (p->parent->right == p);
	      parentp = &p->parent->right;
	      gp_r = 1;
	    }

	  /* P must have a parent since it cannot be the root.  */
	  gp = p->parent;
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
	      p->red = 1;
	      gp->red = 1;
	      root->red = 0;

	      *gparentp = root;
	      root->parent = gp->parent;

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
		  p->left = BTREE_(link_internal) (*rp);
		  if (p->left)
		    p->left->parent = p;

		  *rp = p;
		  p->parent = root;

		  gp->right = *lp;
		  if (gp->right)
		    gp->right->parent = gp;

		  *lp = gp;
		  gp->parent = root;

		  if (! gp->right)
		    gp->right
		      = (BTREE_(node_t) *) ((uintptr_t ) BTREE_(next_hard) (gp)
					    | 1);
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
		  p->right = *lp;
		  if (p->right)
		    p->right->parent = p;

		  *lp = p;
		  p->parent = root;

		  gp->left = BTREE_(link_internal) (*rp);
		  if (gp->left)
		    gp->left->parent = gp;

		  *rp = gp;
		  gp->parent = root;

		  if (! p->right)
		    p->right
		      = (BTREE_(node_t) *) ((uintptr_t ) BTREE_(next_hard) (p)
					    | 1);
		}
	    }
	  else
	    {
	      /* Parent becomes the top of the tree, grandparent and
		 child are its successors.  */
	      p->red = 0;
	      gp->red = 1;

	      *gparentp = p;
	      p->parent = gp->parent;

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
		  gp->left = BTREE_(link_internal) (p->right);
		  if (gp->left)
		    gp->left->parent = gp;
		  p->right = gp;
		  gp->parent = p;
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
		  gp->right = p->left;
		  if (gp->right)
		    gp->right->parent = gp;
		  p->left = gp;
		  gp->parent = p;

		  if (! gp->right)
		    gp->right
		      = (BTREE_(node_t) *) ((uintptr_t) BTREE_(next_hard) (gp)
					    | 1);

		}
	    }
	}

      /* We have changed the color of the root of the tree to red.
	 Making the root node black never changes the black height of
	 the tree, however, it does eliminate a bit of work when both
	 its children are red.  */
      btree->root->red = 0;
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

/* Detach node NODE from the tree BTREE.  */
void
BTREE_(detach) (BTREE_(t) *btree, BTREE_(node_t) *root)
{
  node p, q, r;
  node *rootp;
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
     and means we just have to deal with the successor (the trivial
     case).

     In this example, the successor, UNCHAINED, would be 4 which has
     less than two children.  The child, R, is node 5.  We unchain 4
     and insert R where it was.  Then we replace ROOT with UNCHAINED.
     The resulting tree is thus:
     
                   8
                 /  \
               4     ...
	     /   \
	    1     6
	   / \   / \
	  0   2 5   7

     Assume that we want to remove a node one child, for instance,
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
  r = BTREE_(link_internal) (root->right);
  q = root->left;

  if (q == NULL || r == NULL)
    unchained = root;
  else
    {
      unchained = BTREE_(next) (root);
      assert (!unchained->left || ! BTREE_(link_internal) (unchained->right));
    }

  /* We know that at least the left or right successor of UNCHAINED is
     NULL.  R becomes the other one, it is chained into the parent of
     UNCHAINED.  */
  r = unchained->left;
  if (r == NULL)
    r = BTREE_(link_internal) (unchained->right);
  else
    assert (! BTREE_(link_internal) (unchained->right));

  /* Update ROOT's predecessor (if it has a right thread) to point to
     ROOT's successor.  */
  q = BTREE_(prev) (root);
  if (q && ! BTREE_(link_internal) (q->right))
    q->right = (BTREE_(node_t) *) ((uintptr_t) (unchained == root
						? BTREE_(next) (root)
						: unchained) | 1);

  /* Cache the parent as we may not be able to recover it if R is
     NULL and unchained is clobbered.  */
  if (unchained->parent == root)
    p = unchained;
  else
    p = unchained->parent;

  /* And get UNCHAINED's parent to point to R.  */
  if (! unchained->parent)
    {
      /* UNCHAINED is only ever the root when either there is one or
	 two nodes in the tree.  */
      assert (unchained == root);
      btree->root = r;
    }
  else if (unchained->parent->left == unchained)
    {
      /* UNCHAINED can't be to the immediate left of ROOT as it is
	 ROOT's successor.  */
      assert (unchained->parent != root);
      unchained->parent->left = r;
    }
  else
    {
      assert (unchained->parent->right == unchained);

      if (r)
	unchained->parent->right = r;
      else
	/* R is NULL and this is a right leaf.  */
	unchained->parent->right
	  = (BTREE_(node_t) *) ((uintptr_t) BTREE_(next_hard) (unchained) | 1);
    }

  /* Adjust R's parent pointer (we can't do it earlier as the call to
     BTREE_(next_hard) would fail because we wouldn't have a proper
     tree).  */
  if (r)
    r->parent = p;

  unchained_is_red = unchained->red;

  /* Replace the element to detach with its now unchained
     successor.  */
  if (unchained != root)
    {
      unchained->left = root->left;
      if (unchained->left)
	unchained->left->parent = unchained;

      /* If ROOT->RIGHT is a right thread then it remains correct.  */
      if (BTREE_(link_internal) (root->right))
	{
	  unchained->right = root->right;
	  if (unchained->right)
	    unchained->right->parent = unchained;
	}

      unchained->parent = root->parent;
      unchained->red = root->red;

      *rootp = unchained;
    }

  if (!unchained_is_red)
    {
      /* Now we lost a black edge, which means that the number of black
	 edges on every path is no longer constant.  We must balance the
	 tree.  */
      /* P is the parent of R (now that UNCHAINED has been detached).
	 R is likely to be NULL in the first iteration.  */
      /* NULL nodes are considered black throughout - this is necessary for
	 correctness.  */
      for (; p && (r == NULL || !r->red); p = p->parent)
	{
	  node *pp = selfp (btree, p);

	  assert (! r || r->parent == p);

	  /* Two symmetric cases.  */
	  if (r == p->left)
	    {
	      /* Q is R's brother, P is R's parent.  The subtree with root
		 R has one black edge less than the subtree with root Q.

		             p
			    / \
			   r   q
	       */
	      q = p->right;
	      assert (BTREE_(link_internal) (q));
	      if (q->red)
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
			    r   ql

		   */
		  q->red = 0;
		  p->red = 1;
		  /* Left rotate p.  */
		  q->parent = p->parent;
		  p->right = q->left;
		  if (p->right)
		    p->right->parent = p;

		  q->left = p;
		  p->parent = q;
		  
		  *pp = q;
		  /* Make sure pp is right if the case below tries to use
		     it.  */
		  pp = &q->left;
		  q = p->right;
		  assert (BTREE_(link_internal) (q));

		  if (! p->right)
		    p->right
		      = (BTREE_(node_t) *) ((uintptr_t) BTREE_(next_hard) (p)
					    | 1);
		}
	      /* We know that Q can't be NULL here.  We also know that Q is
		 black.  */
	      assert (! q->red);

	      if ((q->left == NULL || !q->left->red)
		  && (BTREE_(link_internal) (q->right) == NULL
		      || !q->right->red))
		{
		  /* Q has two black successors.  We can simply color Q red.
		     The whole subtree with root P is now missing one black
		     edge.  Note that this action can temporarily make the
		     tree invalid (if P is red).  But we will exit the loop
		     in that case and set P black, which both makes the tree
		     valid and also makes the black edge count come out
		     right.  If P is black, we are at least one step closer
		     to the root and we'll try again the next iteration.  */
		  q->red = 1;
		  r = p;
		}
	      else
		{
		  /* Q is black, one of Q's successors is red.  We can
		     repair the tree with one operation and will exit the
		     loop afterwards.  */
		  if (! BTREE_(link_internal) (q->right) || !q->right->red)
		    {
		      /* The left one is red.  We perform the same action as
			 in maybe_split_for_insert where two red edges are
			 adjacent but point in different directions:
			 Q's left successor (let's call it Q2) becomes the
			 top of the subtree we are looking at, its parent (Q)
			 and grandparent (P) become its successors. The former
			 successors of Q2 are placed below P and Q.
			 P becomes black, and Q2 gets the color that P had.
			 This changes the black edge count only for node R and
			 its successors.

		             p                q2
			    / \             /    \
			   r   q    ->     p      q
                              / \         / \    / \
                             q2          r  q2l
		      */
		      node q2 = q->left;
		      q2->red = p->red;

		      q2->parent = p->parent;

		      p->right = q2->left;
		      if (p->right)
			p->right->parent = p;

		      q->left = BTREE_(link_internal) (q2->right);
		      if (q->left)
			q->left->parent = q;

		      q2->right = q;
		      q->parent = q2;

		      q2->left = p;
		      p->parent = q2;

		      *pp = q2;
		      p->red = 0;

		      if (! p->right)
			p->right = (BTREE_(node_t) *)
			  ((uintptr_t) BTREE_(next_hard) (p) | 1);
		    }
		  else
		    {
		      /* It's the right one.  Rotate P left. P becomes black,
			 and Q gets the color that P had.  Q's right successor
			 also becomes black.  This changes the black edge
			 count only for node R and its successors.

		                q
		               / \
		      ->      p   pr
			     / \
			    r   ql
		       */

		      q->red = p->red;
		      p->red = 0;

		      q->parent = p->parent;

		      q->right->red = 0;

		      /* left rotate p */
		      p->right = q->left;
		      if (p->right)
			p->right->parent = p;

		      q->left = p;
		      p->parent = q;

		      *pp = q;

		      if (! p->right)
			p->right = (BTREE_(node_t) *)
			  ((uintptr_t) BTREE_(next_hard) (p) | 1);
		    }

		  /* We're done.  */
		  return;
		}
	    }
	  else
	    {
	      assert (r == BTREE_(link_internal) (p->right));
	      /* Comments: see above.  */
	      q = p->left;
	      assert (BTREE_(link_internal) (q));
	      if (q != NULL && q->red)
		{
		  q->red = 0;
		  p->red = 1;
		  q->parent = p->parent;
		  p->left = BTREE_(link_internal) (q->right);
		  if (p->left)
		    p->left->parent = p;
		  q->right = p;
		  p->parent = q;
		  *pp = q;
		  pp = &q->right;
		  q = p->left;
		  assert (BTREE_(link_internal) (q));
		}
	      assert (! q->red);
	      if ((! BTREE_(link_internal) (q->right) || !q->right->red)
		       && (q->left == NULL || !q->left->red))
		{
		  q->red = 1;
		  r = p;
		}
	      else
		{
		  if (q->left == NULL || !q->left->red)
		    {
		      node q2 = q->right;
		      q2->red = p->red;
		      q2->parent = p->parent;
		      p->left = BTREE_(link_internal) (q2->right);
		      if (p->left)
			p->left->parent = p;
		      q->right = q2->left;
		      if (q->right)
			q->right->parent = q;
		      q2->left = q;
		      q->parent = q2;
		      q2->right = p;
		      p->parent = q2;
		      *pp = q2;
		      p->red = 0;

		      if (! q->right)
			q->right = (BTREE_(node_t) *)
			  ((uintptr_t) BTREE_(next_hard) (q) | 1);
		    }
		  else
		    {
		      q->red = p->red;
		      p->red = 0;
		      q->left->red = 0;
		      q->parent = p->parent;
		      p->left = BTREE_(link_internal) (q->right);
		      if (p->left)
			p->left->parent = p;
		      q->right = p;
		      p->parent = q;
		      *pp = q;
		    }
		  return;
		}
	    }
	}
      if (r != NULL)
	r->red = 0;
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
