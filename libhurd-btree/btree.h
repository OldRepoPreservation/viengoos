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
#include <stddef.h>

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
struct BTREE_(node_ptr)
{
  union
  {
    struct
    {
      /* Whether PTR is a child (=0) or a thread pointer (=1).  */
      uintptr_t thread : 1;
      /* The value of the pointer >> 1.  */
      uintptr_t ptr : (sizeof (uintptr_t) * 8 - 1);
    };
    uintptr_t raw;
    struct BTREE_(node) *ptr;
  };
};

struct BTREE_(node_pptr)
{
  union
  {
    struct
    {
      /* Whether the node is red (=1) or black (=0).  */
      uintptr_t red : 1;
      /* The value of the pointer >> 1.  */
      uintptr_t ptr : (sizeof (uintptr_t) * 8 - 1);
    };
    uintptr_t raw;
    struct BTREE_(node) *ptr;
  };
};

struct BTREE_(node)
{
  /* All members are private to the implementation.  */
  struct BTREE_(node_pptr) parent;
  struct BTREE_(node_ptr) left;
  struct BTREE_(node_ptr) right;
};
typedef struct BTREE_(node) BTREE_(node_t);

/* Internal accessors functions.  */

/* Give a struct BTREE_(node_ptr), return a BTREE_(node_t).  This
   function does not distinguish between child links and thread
   links.  */
#define BTREE_NP(__bn_node_ptr)				\
  ((BTREE_(node_t) *) ((__bn_node_ptr).ptr << 1))
/* Set a struct BTREE_(node_ptr) * to point to BTREE_(node_t).  */
#define BTREE_NP_SET(__bnp_node_ptrp, __bnp_value)		\
  do								\
    {								\
      BTREE_(node_t) *__bnp_val = (__bnp_value);		\
      (__bnp_node_ptrp)->ptr = ((uintptr_t) __bnp_val) >> 1;	\
    }								\
  while (0)

/* Return whether the BTREE_(node_ptr) contains a thread pointer (if
   not, then it contains a child pointer).  */
#define BTREE_NP_THREAD_P(__bntp_node_ptr)	\
  ((__bntp_node_ptr).thread)
#define BTREE_NP_THREAD_P_SET(__bntps_node_ptrp, __bntps_value)	\
  (((__bntps_node_ptrp)->thread) = __bntps_value)

#define BTREE_NP_THREAD(__bnt_node_ptr)				\
  (BTREE_NP_THREAD_P ((__bnt_node_ptr))				\
   ? BTREE_NP ((__bnt_node_ptr)) : NULL)
#define BTREE_NP_THREAD_SET(__bnts_node_ptrp, __bnts_value)	\
  do								\
    {								\
      struct BTREE_(node_ptr) *__bnts_n = (__bnts_node_ptrp);	\
      BTREE_(node_t) *__bnts_val = (__bnts_value);		\
								\
      BTREE_NP_SET (__bnts_n, __bnts_val);			\
      BTREE_NP_THREAD_P_SET (__bnts_n, 1);			\
    }								\
  while (0)

#define BTREE_NP_CHILD(__bnc_node_ptr)			\
  (BTREE_NP_THREAD_P ((__bnc_node_ptr))			\
   ? NULL : BTREE_NP ((__bnc_node_ptr)))
#define BTREE_NP_CHILD_SET(__bncs_node_ptrp, __bncs_value)	\
  do								\
    {								\
      struct BTREE_(node_ptr) *__bncs_np = (__bncs_node_ptrp);	\
      BTREE_(node_t) *__bncs_val = (__bncs_value);		\
								\
      BTREE_NP_SET (__bncs_np, __bncs_val);			\
      BTREE_NP_THREAD_P_SET (__bncs_np, 0);			\
    }								\
  while (0)

/* Return whether a BTREE_(node) * designates a red node.  */
#define BTREE_NODE_RED_P(__bnrp_node)		\
  ((__bnrp_node)->parent.red)
/* Set whether the BTREE_(node) * designates a red node.  */
#define BTREE_NODE_RED_SET(__bnrs_node, __bnrs_value)	\
  (((__bnrs_node)->parent.red) = __bnrs_value)

/* The root of a btree.  */
typedef struct 
{
  /* All members are private to the implementation.  */
  struct BTREE_(node_ptr) root;
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
  BTREE_NP_CHILD_SET (&btree->root, 0);
}

/* This is a private function do not call it from user code!

   Internal function.  Perform a consistent check on the tree rooted
   at ROOT.  */
#ifndef NDEBUG
extern void BTREE_(check_tree_internal) (BTREE_(t) *btree,
					 BTREE_(node_t) *root,
					 BTREE_(key_compare_t) compare,
					 size_t key_offset, bool check_colors);
#define BTREE_check_tree_internal_(bt, r, c, k, cc)	\
  BTREE_(check_tree_internal) (bt, r, c, k, cc)
#else
#define BTREE_check_tree_internal_(bt, r, c, k, c) do { } while (0)
#endif

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
      /* Are both children red?  */
      || (BTREE_NP_CHILD (node->left)
	  && BTREE_NODE_RED_P (BTREE_NP_CHILD (node->left))
	  && BTREE_NP_CHILD (node->right)
	  && BTREE_NODE_RED_P (BTREE_NP_CHILD (node->right))))
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
   *PARENT (or sets it to NULL if the tree is empty).

   If no node is found in the tree with key KEY, then *PREDP is set to
   the node with the largest key which compares less than KEY or NULL
   if key is smaller than all nodes in the tree.  *SUCCP is set
   similarly to *PREDP expect that it points to the node with the
   smallest key which compares greater than KEY or NULL if key is
   larger than all nodes in the tree.  Returns 0 if found and ESRCH
   otherwise.

   If MAY_OVERLAP is true, then finds a slot to insert a node with key
   KEY even if such a node already exists.  */
BTREE_EXTERN_INLINE error_t
BTREE_(find_internal) (BTREE_(t) *btree, BTREE_(key_compare_t) compare,
		       size_t key_offset, const void *key,
		       struct BTREE_(node_ptr) **nodepp,
		       BTREE_(node_t) **predp, BTREE_(node_t) **succp,
		       BTREE_(node_t) **parentp, bool may_overlap);
BTREE_EXTERN_INLINE error_t
BTREE_(find_internal) (BTREE_(t) *btree, BTREE_(key_compare_t) compare,
		       size_t key_offset, const void *key,
		       struct BTREE_(node_ptr) **nodepp,
		       BTREE_(node_t) **predp, BTREE_(node_t) **succp,
		       BTREE_(node_t) **parentp, bool may_overlap)
{
  int r;
  error_t err = ESRCH;
  struct BTREE_(node_ptr) *nodep = &btree->root;
  *parentp = 0;
  *predp = *succp = 0;

  while (BTREE_NP_CHILD (*nodep))
    {
      /* We need NODE not because it is convenient but because if any
	 rotations are done by BTREE_(maybe_split_internal) then the
	 value it contains may be invalid.  */
      BTREE_(node_t) *node = BTREE_NP (*nodep);

      r = compare (key, (void *) node + key_offset);
      if (r == 0 && ! may_overlap)
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
  struct BTREE_(node_ptr) *nodep;
  BTREE_(node_t) *node, *pred, *succ, *parent;

  err = BTREE_(find_internal) (btree, compare, key_offset, key,
			       &nodep, &pred, &succ, &parent, false);
  node = BTREE_NP_CHILD (*nodep);
  /* If not found, NODE will be NULL.  */
  assert ((err == ESRCH && ! node) || (err == 0 && node));
  return node;
}

/* Insert node NODE into btree BTREE.  COMPARE is the comparison
   function.  NEWNODE's key must be valid.  If MAY_OVERLAP is not
   true, and there exists a node with a key that compares equal to
   NEWNODE's key, such a node is returned.  Otherwise, returns 0.  */
BTREE_EXTERN_INLINE BTREE_(node_t) *
BTREE_(insert) (BTREE_(t) *btree, BTREE_(key_compare_t) compare,
		size_t key_offset, bool may_overlap, BTREE_(node_t) *newnode);
BTREE_EXTERN_INLINE BTREE_(node_t) *
BTREE_(insert) (BTREE_(t) *btree, BTREE_(key_compare_t) compare,
		size_t key_offset, bool may_overlap, BTREE_(node_t) *newnode)
{
  error_t err;
  struct BTREE_(node_ptr) *nodep;
  BTREE_(node_t) *pred, *succ, *parent;

  assert (! newnode->left.raw);
  assert (! newnode->right.raw);
  assert (! newnode->parent.raw);

  err = BTREE_(find_internal) (btree, compare, key_offset,
			       (void *) newnode + key_offset,
			       &nodep, &pred, &succ, &parent,
			       may_overlap);
  if (! err)
    /* Overlap!  */
    {
      assert (! BTREE_NP_THREAD_P (*nodep));
      return BTREE_NP_CHILD (*nodep);
    }

  assert (err == ESRCH);
  assert (BTREE_NP_CHILD (*nodep) == NULL);

  /* A node with the same key as NEWNODE does not exist in the tree
     BTREE.  NODEP is a pointer to the location where NODE should be
     installed (i.e. either &PARENT->LEFT or &PARENT->RIGHT).  PARENT
     is the parent of *NODEP.  */
  BTREE_NP_CHILD_SET (nodep, newnode);
  BTREE_NP_SET (&newnode->parent, parent);
  BTREE_NP_THREAD_SET (&newnode->left, pred);
  BTREE_NP_THREAD_SET (&newnode->right, succ);
  BTREE_NODE_RED_SET (newnode, true);

  if (pred && BTREE_NP_THREAD_P (pred->right))
    /* NEWNODE's predecessor has a right thread, update it.  */
    {
      assert (BTREE_NP_CHILD (pred->right) == succ);
      BTREE_NP_THREAD_SET (&pred->right, newnode);
    }

  if (succ && (BTREE_NP_THREAD_P (succ->left)
	       || ! BTREE_NP_CHILD (succ->left)))
    /* NEWNODE's successor has a left thread, update it.  */
    {
      if (BTREE_NP_THREAD_P (succ->left))
	assert (BTREE_NP_CHILD (succ->left) == pred);
      BTREE_NP_THREAD_SET (&succ->left, newnode);
    }

  if (parent)
    /* There may be two red edges in a row now, which we must avoid by
       rotating the tree.  */
    BTREE_(maybe_split_internal) (btree, newnode, 1);
  else
    /* NEWNODE is the top of the tree.  Making it black means less
       balancing later on.  */
    BTREE_NODE_RED_SET (newnode, false);

  BTREE_check_tree_internal_ (btree, BTREE_NP_CHILD (btree->root),
			      compare, key_offset, true);

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

  node = BTREE_NP (btree->root);

  if (! node)
    return NULL;

  while (BTREE_NP_CHILD (node->left))
    node = BTREE_NP_CHILD (node->left);
  return node;
}

/* Internal function, do not call from user code.  */
extern BTREE_(node_t) *BTREE_(next_hard) (BTREE_(node_t) *node);
/* Internal function, do not call from user code.  */
extern BTREE_(node_t) *BTREE_(prev_hard) (BTREE_(node_t) *node);

/* Return the node following node NODE or NULL if NODE is the last
   (i.e. largest) node in the tree.  This function is guaranteed to
   never touch a node which is lexically less than NODE.  */
BTREE_EXTERN_INLINE BTREE_(node_t) *BTREE_(next) (BTREE_(node_t) *node);
BTREE_EXTERN_INLINE BTREE_(node_t) *
BTREE_(next) (BTREE_(node_t) *node)
{
  if (BTREE_NP_THREAD_P (node->right))
    /* We have a right thread, use it.  */
    return BTREE_NP_THREAD (node->right);
  assert (BTREE_NP_CHILD (node->right));

  /* NODE has a right child node.  The left most child of NODE->RIGHT
     is the next node.  */
  node = BTREE_NP_CHILD (node->right);
  while (BTREE_NP_CHILD (node->left))
    node = BTREE_NP_CHILD (node->left);
  return node;
}

/* Return the node preceding node NODE or NULL if NODE is the first
   (i.e. smallest) node in the tree.  */
BTREE_EXTERN_INLINE BTREE_(node_t) *BTREE_(prev) (BTREE_(node_t) *node);
BTREE_EXTERN_INLINE BTREE_(node_t) *
BTREE_(prev) (BTREE_(node_t) *node)
{
  if (BTREE_NP_THREAD_P (node->left))
    /* We have a left thread, use it.  */
    return BTREE_NP_THREAD (node->left);

  /* We have to do it the hard way.  */
  BTREE_(node_t) *prev = BTREE_(prev_hard) (node);

  if (! BTREE_NP_CHILD (node->left))
    BTREE_NP_THREAD_SET (&node->left, prev);

  return prev;
}

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
                  my_int_node_compare, false)
     
     int
     int_node_compare (const int *a, const int *b)
     {
       return *a - *b;
     }

   Would make btree_int_node_find, btree_int_node_insert, etc.
   available.  */
#define BTREE_CLASS(name, node_type, key_type, key_field,		\
		    btree_node_field, cmp_function, may_overlap)	\
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
  return (node_type *)							\
    BTREE_(insert) (&btree->btree,					\
		    (int (*) (const void *, const void *)) cmp,		\
		    offsetof (node_type, key_field)			\
		    - offsetof (node_type, btree_node_field),		\
		    may_overlap, &newnode->btree_node_field);		\
}									\
									\
static inline void							\
BTREE_(name##_detach) (BTREE_(name##_t) *btree, node_type *node)	\
{									\
  BTREE_(detach) (&btree->btree, &node->btree_node_field);		\
									\
  int (*cmp) (const key_type *, const key_type *) = (cmp_function);	\
  BTREE_check_tree_internal_ (&btree->btree,				\
			      BTREE_NP_CHILD (btree->btree.root),	\
			      (BTREE_(key_compare_t)) cmp,		\
			      offsetof (node_type, key_field)		\
			      - offsetof (node_type, btree_node_field), \
			      true);					\
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
