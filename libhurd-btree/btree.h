/* Balanced tree insertion and deletion routines.
   Copyright (C) 2004, 2007 Free Software Foundation, Inc.
   Written by Neal H. Walfield <neal@gnu.org>.

   This file is part of the GNU Hurd.

   The GNU Hurd is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.

   The GNU Hurd is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.  */

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

#ifndef BTREE_H
#define BTREE_H	1

#include <stdint.h>
#include <stdbool.h>
#include <errno.h>
#include <assert.h>

#ifndef BTREE_EXTERN_INLINE
# define BTREE_EXTERN_INLINE extern inline
#endif

#ifndef offsetof
#ifdef __compiler_offsetof
#define offsetof(TYPE,MEMBER) __compiler_offsetof(TYPE,MEMBER)
#else
#define offsetof(TYPE, MEMBER) ((size_t) &((TYPE *)0)->MEMBER)
#endif
#endif

/* If BTREE_NAME_PREFIX is set then all functions and types will its
   value plus _ prefixed to their names.  For example, setting it to
   hurd means that rather than the type btree_t being exposed,
   hurd_btree_t is exposed.  */
#define BTREE_NAME_PREFIX hurd
#ifdef BTREE_NAME_PREFIX
#define BTREE_CONCAT2(a,b) a##b
#define BTREE_CONCAT(a,b) BTREE_CONCAT2(a,b)
#define BTREE_(name) BTREE_CONCAT(BTREE_NAME_PREFIX,_btree_##name)
#else
#define BTREE_(name) btree_##name
#endif

/* A node in a btree.  User code will typically have a structure of
   its own and include this as one of the fields.  For instance, a
   tree indexed by integers might have a node structure which looks
   like this:

     struct int_node
     {
       btree_node_t btree_node;
       int key;
       ...
     };

   The btree node structure does not need to be the first element in
   the structure.

   This is useful as the user code has complete control over the way
   memory is allocated and deallocated and it eliminates a level of
   indirection where user data is pointed to by the node which is
   present in many btree implementations.  */
struct BTREE_(node)
{
  /* All members are private to the implementation.  */
  struct BTREE_(node) *left;
  /* If the least significant bit is 0, right is not a right link but
     a right thread (i.e. a pointer to the current node's
     successor).  */
  struct BTREE_(node) *right;
  struct BTREE_(node) *parent;
  bool red;
};
typedef struct BTREE_(node) BTREE_(node_t);

/* The root of a btree.  */
typedef struct 
{
  /* All members are private to the implementation.  */
  struct BTREE_(node) *root;
} BTREE_(t);

/* Compare two keys.  Return 0 if A and B are equal, less then 0 if a
   < b or greater than 0 if a > b.  (NB: it is possible to create
   comparison functions which do not branch.  For instance, with
   integers keys, one can do:

     return *(int *)a - *(int *)b

   instead of using two if statements.)  */
typedef int (*BTREE_(key_compare_t)) (const void *a, const void *b);

/* Initialize a btree data structure.  (NB: nodes needn't be
   initialized.)  */
BTREE_EXTERN_INLINE void BTREE_(tree_init) (BTREE_(t) *btree);
BTREE_EXTERN_INLINE void
BTREE_(tree_init) (BTREE_(t) *btree)
{
  btree->root = 0;
}

/* This is a private function do not call it from user code!

   Given a node pointer return the link portion (i.e. return NULL if
   LINK is a thread and not a child pointer).  */
static inline BTREE_(node_t) *
BTREE_(link_internal) (BTREE_(node_t) *link)
{
  if ((((uintptr_t) link) & 0x1) == 1)
    /* This is a right thread, not a child.  */
    return NULL;
  return link;
}

/* This is a private function do not call it from user code!

   Possibly "split" a node with two red successors, and/or fix up two
   red edges in a row.  If FORCE is 1, we always do the split
   (anticipating an insertion).  */
extern void BTREE_(maybe_split2_internal) (BTREE_(t) *tree,
					   BTREE_(node_t) *node,
					   int force);

/* This is a private function do not call it from user code!

   This evaluates the "possibly" predicate described above.  If it
   true then we make the actual function call and do the real work
   there.  */
BTREE_EXTERN_INLINE void BTREE_(maybe_split_internal) (BTREE_(t) *tree,
						       BTREE_(node_t) *node,
						       int force);
BTREE_EXTERN_INLINE void
BTREE_(maybe_split_internal) (BTREE_(t) *tree, BTREE_(node_t) *node,
			      int force)
{
  if (force
      || (node->left && node->left->red
	  && BTREE_(link_internal) (node->right)
	  && node->right->red))
    BTREE_(maybe_split2_internal) (tree, node, force);
}

/* This is a private function do not call it from user code!

   Search the tree BTREE for the node with key KEY.  COMPARE is the
   comparison function to use to compare keys.  KEY_OFFSET is the
   location of the key in bytes relative to the address of a node.

   Returns the address of the pointer to the node (or where it would
   be if it was in the tree) in *NODEPP, e.g. if KEY is larger than
   the parent's, *NODEPP is set to &(*parent)->right.  Returns
   (whether or not the node with KEY is found) the parent node in
   *PARENT (or sets it to NULL if the tree is empty).  If no node is
   found in the tree with key KEY, then *PREDP is set to the node with
   the largest key which compares less than KEY or NULL if key is
   smaller than all nodes in the tree.  *SUCCP is set similarly to
   *PREDP expect that it points to the node with the smallest key
   which compares greater than KEY or NULL if key is larger than all
   nodes in the tree.  Returns 0 if found and ESRCH otherwise.  */
BTREE_EXTERN_INLINE error_t
BTREE_(find_internal) (BTREE_(t) *btree, BTREE_(key_compare_t) compare,
		       size_t key_offset, const void *key,
		       BTREE_(node_t) ***nodepp, BTREE_(node_t) **predp,
		       BTREE_(node_t) **succp, BTREE_(node_t) **parentp);
BTREE_EXTERN_INLINE error_t
BTREE_(find_internal) (BTREE_(t) *btree, BTREE_(key_compare_t) compare,
		       size_t key_offset, const void *key,
		       BTREE_(node_t) ***nodepp, BTREE_(node_t) **predp,
		       BTREE_(node_t) **succp, BTREE_(node_t) **parentp)
{
  int r;
  error_t err = ESRCH;
  BTREE_(node_t) **nodep = &btree->root;
  *parentp = NULL;
  *predp = *succp = NULL;

  while (BTREE_(link_internal) (*nodep))
    {
      /* We need NODE not because it is convenient but because if any
	 rotations are done by BTREE_(maybe_split_internal) then the
	 value it contains may be invalid.  */
      BTREE_(node_t) *node = *nodep;

      r = compare (key, (void *) node + key_offset);
      if (r == 0)
	/* Found it.  */
	{
	  err = 0;
	  break;
	}

      BTREE_(maybe_split_internal) (btree, node, 0);
      /* If that did any rotations NODEP may now be garbage.  That
	 doesn't matter, because the value it contains is never used
	 again in that case.  */

      /* Node with key KEY is on the left (r < 0) or right (r > 0).  */
      *parentp = node;
      nodep = r < 0 ? &node->left : &node->right;

      /* NODE's key is largest key smaller than KEY (r > 0) or the
	 smallest key larger than KEY we have seen so far (r < 0).  In
	 which case it is either the potential predecessor or
	 successor respectively.  */
      *(r < 0 ? succp : predp) = node;
    }

  *nodepp = nodep;
  return err;
}

/* Search the tree BTREE for the node with key KEY.  COMPARE is the
   comparison function to use to compare keys.  KEY_OFFSET is the
   location of a key in bytes relative to the node.  Returns the node
   corresponding to KEY if it exists in the tree.  Otherwise, returns
   NULL.  */
BTREE_EXTERN_INLINE BTREE_(node_t) *
BTREE_(find) (BTREE_(t) *btree, BTREE_(key_compare_t) compare,
	      size_t key_offset, const void *key);
BTREE_EXTERN_INLINE BTREE_(node_t) *
BTREE_(find) (BTREE_(t) *btree, BTREE_(key_compare_t) compare,
	      size_t key_offset, const void *key)
{
  error_t err;
  BTREE_(node_t) **nodep, *node, *pred, *succ, *parent;

  err = BTREE_(find_internal) (btree, compare, key_offset, key,
			       &nodep, &pred, &succ, &parent);
  node = BTREE_(link_internal) (*nodep);
  /* If not found, NODE will be NULL.  */
  assert ((err == ESRCH && ! node) || (err == 0 && node));
  return node;
}

/* Insert node NODE into btree BTREE.  COMPARE is the comparison
   function.  NEWNODE's key must be valid.  If the node cannot be
   inserted as a node with the key already exists, returns the node.
   Otherwise, returns 0.  */
BTREE_EXTERN_INLINE BTREE_(node_t) *
BTREE_(insert) (BTREE_(t) *btree, BTREE_(key_compare_t) compare,
		size_t key_offset, BTREE_(node_t) *newnode);
BTREE_EXTERN_INLINE BTREE_(node_t) *
BTREE_(insert) (BTREE_(t) *btree, BTREE_(key_compare_t) compare,
		size_t key_offset, BTREE_(node_t) *newnode)
{
  error_t err;
  BTREE_(node_t) **nodep, *pred, *succ, *parent;

  err = BTREE_(find_internal) (btree, compare, key_offset,
			       (void *) newnode + key_offset,
			       &nodep, &pred, &succ, &parent);
  if (! err)
    /* Overlap!  Bail.  */
    return BTREE_(link_internal) (*nodep);

  assert (err == ESRCH);
  assert (BTREE_(link_internal) (*nodep) == NULL);

  /* A node with the same key as NEWNODE does not exist in the tree
     BTREE.  NODEP is a pointer to the location where NODE should be
     installed (i.e. either &PARENT->LEFT or &PARENT->RIGHT).  PARENT
     is the parent of *NODEP.  */
  *nodep = newnode;
  newnode->parent = parent;
  newnode->left = NULL;
  newnode->right = (BTREE_(node_t) *) ((uintptr_t) succ | 1);
  newnode->red = 1;

  if (pred && ((uintptr_t) pred->right & 1) == 1)
    /* NEWNODE's predecessor has a right thread.  */
    {
      assert (pred->right == (BTREE_(node_t) *) ((uintptr_t) succ | 1));
      /* But more importantly, we must point it to us.  */
      pred->right = (BTREE_(node_t) *) (((uintptr_t) newnode) | 1);
    }

  if (parent)
    /* There may be two red edges in a row now, which we must avoid by
       rotating the tree.  */
    BTREE_(maybe_split_internal) (btree, newnode, 1);
  else
    /* NEWNODE is the top of the tree.  Making it black means less
       balancing later on.  */
    newnode->red = 0;

  return 0;
}

/* Detach node NODE from the tree BTREE.

   Since the btree implementation never allocates memory on its but
   relies on the caller to do so, this function only removes the node
   from the tree.  If the entire tree is being deleted, then this
   function need not be called on each node individually: this is only
   a waste of time.  Instead, one can do:

     btree_node_t node, next;
     for (node = btree_first (btree); node; node = next)
       {
         next = btree_next (node);
         free (node);
       }

   Since btree_next (node) is guaranteed to never touch a node which
   is lexically less than NODE.  */
extern void BTREE_(detach) (BTREE_(t) *btree, BTREE_(node_t) *node);

/* Return the node with the smallest key in tree BTREE or NULL if the
   tree is empty.  */
BTREE_EXTERN_INLINE BTREE_(node_t) *BTREE_(first) (BTREE_(t) *btree);
BTREE_EXTERN_INLINE BTREE_(node_t) *
BTREE_(first) (BTREE_(t) *btree)
{
  BTREE_(node_t) *node;

  node = btree->root;

  if (! node)
    return NULL;

  while (node->left)
    node = node->left;
  return node;
}

/* Return the node following node NODE or NULL if NODE is the last
   (i.e. largest) node in the tree.  This function is guaranteed to
   never touch a node which is lexically less than NODE.  */
BTREE_EXTERN_INLINE BTREE_(node_t) *BTREE_(next) (BTREE_(node_t) *node);
BTREE_EXTERN_INLINE BTREE_(node_t) *
BTREE_(next) (BTREE_(node_t) *node)
{
  if (((uintptr_t) node->right & 1) == 1)
    /* We have a right thread, use it.  */
    return (BTREE_(node_t) *) (((uintptr_t) node->right) & ~1);
  assert (node->right);

  /* NODE has a right child node.  The left most child of NODE->RIGHT
     is the next node.  */
  node = node->right;
  while (node->left)
    node = node->left;
  return node;
}

/* Return the node preceding node NODE or NULL if NODE is the first
   (i.e. smallest) node in the tree.  */
extern BTREE_(node_t) *BTREE_(prev) (BTREE_(node_t) *node);

/* Create a more strongly typed btree interface a la C++'s templates.

   NAME is the name of the new btree class.  NAME will be used to
   create names for the class type and methods.  The names shall be
   created using the following rule: the btree_ namespace will prefix
   all methods followed by NAME followed by an underscore and then the
   method name.  Assuming BTREE_NAME_PREFIX is undefined, the
   following names are thus exposed:

    Types:
     btree_NAME_t;

    Functions:
     void btree_NAME_tree_init (btree_NAME_t *btree, NODE_TYPE *node);
     NODE_TYPE *btree_NAME_find (btree_NAME_t *btree, const KEY_TYPE *key);
     NODE_TYPE *btree_NAME_insert (btree_NAME_t *btree, NODE_TYPE *newnode);
     void btree_NAME_detach (btree_NAME_t *btree, NODE_TYPE *node);
     NODE_TYPE *btree_NAME_first (btree_NAME_t *btree);
     NODE_TYPE *btree_NAME_next (NODE_TYPE *node);
     NODE_TYPE *btree_NAME_prev (NODE_TYPE *node);

   NODE_TYPE is the type of the node.  If you are using a btree keyed
   by integers, you might have a structure similar to the following:

     struct my_int_node
     {
       int key;
       btree_node_t node;
       ...
     };

   In this case, the value of the argument NODE_TYPE would be struct
   my_int_node.

   KEY_TYPE is the type of the key.  Continuing the above example,
   the value of the argument KEY_TYPE would be int.

   A user node structure must contain the key at a fixed offset.
   KEY_FIELD is the name of the field in the structure.  In the
   preceding example, that would be key.

   A node structure must contain a btree_node_t structure.
   BTREE_NODE_FIELD is the name of that field within NODE_TYPE.
   According to the above structure, the value of the argument would
   be node.

   CMP_FUNCTION is a function to compare two keys and is of type:

    int (*) (const KEY_TYPE *a, const KEY_TYPE *b)

   The function is to return 0 if a and b are equal, less then 0 if a
   < b or greater than 0 if a > b.

     extern int my_int_node_compare (const int *a, const int *b);
     
     struct my_int_node
     {
       int key;
       btree_node_t node;
     };
     
     BTREE_CLASS (int, struct my_int_node, int, key, node,
                  my_int_node_compare)
     
     int
     int_node_compare (const int *a, const int *b)
     {
       return *a - *b;
     }

   Would make btree_int_node_find, btree_int_node_insert, etc.
   available.  */
#define BTREE_CLASS(name, node_type, key_type, key_field,		\
		    btree_node_field, cmp_function)			\
									\
typedef struct								\
{									\
  BTREE_(t) btree;							\
} BTREE_(name##_t);							\
									\
static inline void							\
BTREE_(name##_tree_init) (BTREE_(name##_t) *btree)			\
{									\
  BTREE_(tree_init) (&btree->btree);					\
}									\
									\
static inline node_type *						\
BTREE_(name##_find) (BTREE_(name##_t) *btree, const key_type *key)	\
{									\
  int (*cmp) (const key_type *, const key_type *) = (cmp_function);	\
  void *n = BTREE_(find) (&btree->btree,				\
			  (int (*) (const void *, const void *)) cmp,	\
			  offsetof (node_type, key_field)		\
			  - offsetof (node_type, btree_node_field),	\
			  (const void *) key);				\
									\
  return n ? n - offsetof (node_type, btree_node_field) : NULL;		\
}									\
									\
static inline node_type *						\
BTREE_(name##_insert) (BTREE_(name##_t) *btree, node_type *newnode)	\
{									\
  int (*cmp) (const key_type *, const key_type *) = (cmp_function);	\
									\
  return BTREE_(insert) (&btree->btree,					\
			 (int (*) (const void *, const void *)) cmp,	\
			 offsetof (node_type, key_field)		\
			 - offsetof (node_type, btree_node_field),	\
			 &newnode->btree_node_field);			\
}									\
									\
static inline void							\
BTREE_(name##_detach) (BTREE_(name##_t) *btree, node_type *node)	\
{									\
  BTREE_(detach) (&btree->btree, &node->btree_node_field);		\
}									\
									\
static inline node_type *						\
BTREE_(name##_first) (BTREE_(name##_t) *btree)				\
{									\
  void *n = BTREE_(first) (&btree->btree);				\
  return n ? n - offsetof (node_type, btree_node_field) : NULL;		\
}									\
									\
static inline node_type *						\
BTREE_(name##_next) (node_type *node)					\
{									\
  void *n = BTREE_(next) (&node->btree_node_field);			\
  return n ? n - offsetof (node_type, btree_node_field) : NULL;		\
}									\
									\
static inline node_type *						\
BTREE_(name##_prev) (node_type *node)					\
{									\
  void *n = BTREE_(prev) (&node->btree_node_field);			\
  return n ? n - offsetof (node_type, btree_node_field) : NULL;		\
}

#endif /* BTREE_H */
