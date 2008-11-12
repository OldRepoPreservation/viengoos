/* list.h - Linked list interface.
   Copyright (C) 2007, 2008 Free Software Foundation, Inc.
   Written by Neal H. Walfield <neal@gnu.org>.

   This file is part of the GNU Hurd.

   The GNU Hurd is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 3 of the
   License, or (at your option) any later version.

   The GNU Hurd is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see
   <http://www.gnu.org/licenses/>.  */

#ifndef RM_LIST_H
#define RM_LIST_H 1

/* A circular linked-list implementation.  */

/* New nodes must be initialized to NULL before being added to a list.
   (list_unlink will clear them.)  */
struct list_node
{
  struct list_node *next;
  struct list_node *prev;
};

/* HEAD points to the head node.  HEAD's previous pointer is a
   sentinel (see below).  It points to the list's tail.  The list's
   tail's next pointer is also a sentinel.  It points to the list
   header, not the head of the list.  */
struct list
{
  /* Head of the list.  */
  struct list_node *head;
  /* The number of items on the list.  */
  int count;
#ifndef NDEBUG
  const char *name;
#endif
};
typedef struct list list_t;

/* Return whether a node is attached to a list.  */
static inline bool
list_node_attached (struct list_node *node)
{
  return !! node->next;
}

/* Initialize a list.  Equivalently, zero initialization is
   sufficient.  */
static inline void
list_init (struct list *list, const char *name)
{
  list->head = NULL;
  list->count = 0;
#ifndef NDEBUG
  list->name = name;
#endif
}

/* Return whether the pointer is a sentinel.  The head's previous
   pointer is marked as is the tail's next pointer.  All other
   pointers are clear.  */
#define LIST_SENTINEL_P(__ls_ptr) (((uintptr_t) (__ls_ptr) & 1) == 1)

/* Given a pointer, create a sentinel.  */
#define LIST_SENTINEL(__ls_ptr)				\
  ((struct list_node *) ((uintptr_t) (__ls_ptr) | 1))

/* Return the pointer value clearing any sentinel.  */
#define LIST_PTR(__ls_ptr)				\
  ((struct list_node *) ((uintptr_t) (__ls_ptr) & ~1))

/* Return LIST's head.  If the list is empty, returns NULL.  */
static inline struct list_node *
list_head (struct list *list)
{
  return list->head;
}

/* Return LIST's tail.  If the list is empty, returns NULL.  */
static inline struct list_node *
list_tail (struct list *list)
{
  if (! list->head)
    return NULL;
  assert (LIST_SENTINEL_P (list->head->prev));
  /* TAIL->NEXT should point to LIST.  */
  assert (LIST_PTR (LIST_PTR (list->head->prev)->next) == (void *) list);
  return LIST_PTR (list->head->prev);
}

/* Returns the item following NODE.  If NODE is the tail of the list
   returns NULL.  */
static inline struct list_node *
list_next (struct list_node *node)
{
  if (LIST_SENTINEL_P (node->next))
    return NULL;
  return node->next;
}

/* Returns the item preceding NODE.  If NODE is the head of the list
   returns NULL.  */
static inline struct list_node *
list_prev (struct list_node *node)
{
  if (LIST_SENTINEL_P (node->prev))
    return NULL;
  return node->prev;
}

/* Return the number of items attach to list LIST.  */
static inline int
list_count (struct list *list)
{
#ifndef NCHECK
  int count = 0;
  struct list_node *node;
  for (node = list_head (list); node; node = list_next (node))
    count ++;

  assert (count == list->count);
#endif

  return list->count;
}

/* Add ITEM to the head of the list LIST.  */
static inline void
list_push (struct list *list, struct list_node *item)
{
  /* We require that when an item is added to a list that its next and
     previous pointers be NULL.  This helps to catch when an item is
     already on a list.  */
  assert (! item->next);
  assert (! item->prev);

  if (list->head)
    {
      assert (LIST_SENTINEL_P (list->head->prev));
      struct list_node *tail = LIST_PTR (list->head->prev);

      /* TAIL->NEXT should point at LIST.  */
      assert (LIST_SENTINEL_P (tail->next));
      assert (LIST_PTR (tail->next) == (void *) list);

      /* tail <-> item (new head) <-> old head.  */
      item->next = list->head;
      item->prev = LIST_SENTINEL (tail);
      list->head->prev = item;
      /* tail->next remains pointing at LIST.  */
    }
  else
    /* List is empty.  */
    {
      assert (list->count == 0);

      item->next = LIST_SENTINEL (list);
      item->prev = LIST_SENTINEL (item);
    }

  /* Make ITEM new head.  */
  list->head = item;
  list->count ++;
}

/* Add ITEM to the end of the list LIST.  */
static inline void
list_enqueue (struct list *list, struct list_node *item)
{
  /* We require that when an item is added to a list that its next and
     previous pointers be NULL.  This helps to catch when an item is
     already on a list.  */
  assert (! item->next);
  assert (! item->prev);

  if (list->head)
    {
      assert (LIST_SENTINEL_P (list->head->prev));
      struct list_node *tail = LIST_PTR (list->head->prev);

      /* TAIL->NEXT should point at LIST.  */
      assert (LIST_SENTINEL_P (tail->next));
      assert (LIST_PTR (tail->next) == (void *) list);

      /* old tail <-> item (new tail) <-> head.  */
      item->next = LIST_SENTINEL (list);
      item->prev = tail;
      list->head->prev = LIST_SENTINEL (item);
      tail->next = item;
    }
  else
    /* List is empty.  */
    {
      assert (list->count == 0);

      item->next = LIST_SENTINEL (list);
      item->prev = LIST_SENTINEL (item);

      /* Make ITEM new head.  */
      list->head = item;
    }

  list->count ++;
}

/* Remove the first item in list LIST and return it.  If the list is
   empty, returns NULL.  */
static inline struct list_node *
list_dequeue (struct list *list)
{
  if (! list->head)
    /* List is empty.  */
    {
      assert (list->count == 0);
      return NULL;
    }

  struct list_node *item = list_head (list);
  assert (item);
  assert (LIST_SENTINEL_P (item->prev));

  /* The new head.  */
  struct list_node *head = list_next (item);
  list->head = head;

#ifndef NDEBUG
  /* The current tail.  It remains pointing at LIST.  */
  struct list_node *tail = LIST_PTR (item->prev);
  assert (LIST_PTR (tail->next) == (void *) list);
#endif

  if (! head)
    /* The list contained a single element.  It is now empty.  */
    {
      /* Next pointers: list -> item -> list.  */
      assert (LIST_SENTINEL_P (item->next));
      assert (LIST_PTR (item->next) == (void *) list);
      /* Previous pointers: item <- item.  */
      assert (LIST_PTR (item->prev) == item);

      assert (list->count == 1);
    }
  else
    /* Update the new head's previous pointer to point at the list's
       tail.  */
    head->prev = item->prev;

  list->count --;

#ifndef NDEBUG
  item->next = item->prev = NULL;
#endif

  return item;
}

/* Insert ITEM after node NODE.  If NODE is NULL, insert at the head
   of the list.  */
static inline void
list_insert_after (struct list *list, struct list_node *item,
		   struct list_node *node)
{
  /* We require that when an item is added to a list that its next and
     previous pointers be NULL.  This helps to catch when an item is
     already on a list.  */
  assert (! item->next);
  assert (! item->prev);

  if (! node)
    return list_push (list, item);

  item->next = node->next;
  node->next = item;
  item->prev = node;

  if (LIST_SENTINEL_P (item->next))
    /* Item is the new last item on the list.  Update the head of the
       list's previous pointer to point to it.  */
    {
      assert (LIST_SENTINEL_P (list->head->prev));
      assert (LIST_PTR (list->head->prev) == node);

      list->head->prev = LIST_SENTINEL (item);
    }
  else
    item->next->prev = item;

  list->count ++;
}

static inline void
list_unlink (struct list *list, struct list_node *item)
{
  /* These cannot be NULL if ITEM is attached to a list.  */
  assert (item->next);
  assert (item->prev);

  /* Ensure that ITEM appears on LIST.  */
#ifndef NCHECK
  assertx (({
	struct list_node *i = item;
	while (! LIST_SENTINEL_P (i->next))
	  i = LIST_PTR (i->next);
	assert (LIST_SENTINEL_P (i->next));

	if (LIST_PTR (i->next) != (void *) list)
	  debug (0, "Item %p appears on %p (%s) not %p (%s)",
		 item, LIST_PTR (i->next),
		 ((struct list *) LIST_PTR (i->next))->name,
		 list, list->name);
	LIST_PTR (i->next) == (void *) list;
      }), "list: %p (%s) (%d), item: %p",
    list, list->name, list_count (list), item);
#endif

  if (LIST_SENTINEL_P (item->next) && LIST_SENTINEL_P (item->prev))
    /* The only item on the list.  */
    {
      assert (list->count == 1);
      assert (list->head == item);
      assert (LIST_PTR (item->next) == (void *) list);

      list->head = NULL;
    }
  else if (LIST_SENTINEL_P (item->prev))
    /* ITEM is the head of the list.  The tail's next pointer is
       remains up to date.  */
    {
      assert (LIST_SENTINEL_P (LIST_PTR (item->prev)->next));
      assert (LIST_PTR (LIST_PTR (item->prev)->next) == (void *) list);

      list->head = item->next;
      item->next->prev = item->prev;
    }
  else if (LIST_SENTINEL_P (item->next))
    /* ITEM is the tail of the list.  */
    {
      assert (LIST_PTR (item->next) == (void *) list);

      list->head->prev = LIST_SENTINEL (item->prev);
      item->prev->next = item->next;
    }
  else
    {
      item->next->prev = item->prev;
      item->prev->next = item->next;
    }

  item->next = NULL;
  item->prev = NULL;

  list->count --;
}

/* Move the list designated by SOURCE to be designated by TARGET.  */
static inline void
list_move (struct list *target, struct list *source)
{
  assert (! target->head);
  assert (target->count == 0);

  *target = *source;

  if (target->head)
    /* Update tail->next to point to the right list.  */
    LIST_PTR (target->head->prev)->next = LIST_SENTINEL (target);

  source->head = NULL;
  source->count = 0;
}

/* Append list SOURCE to the end of the list TARGET.  */
static inline void
list_join (struct list *target, struct list *source)
{
  if (! source->head)
    return;

  if (! target->head)
    return list_move (target, source);

  /* target's tail <-- (1) (2) --> source's head */
  /* source's tail <-- (3) (4) --> target's head */

  /* 2) Connect TARGET's tail to SOURCE's head.  */
  LIST_PTR (target->head->prev)->next = source->head;
  /* 4) Connect SOURCE's tail to TARGET.  */
  LIST_PTR (source->head->prev)->next = LIST_SENTINEL (target);

  struct list_node *sources_tail = LIST_PTR (source->head->prev);
  /* 1) Connect SOURCE's head to TARGET's tail.  */
  source->head->prev = LIST_PTR (target->head->prev);
  /* 3) Connect TARGET's head to SOURCE's tail.  */
  target->head->prev = LIST_SENTINEL (sources_tail);

  source->head = NULL;
  target->count += source->count;
  source->count = 0;
}

/* Instantiate a type-strong list class.

   Given the following:

     struct foo
     {
       ...;
       struct list_node node;
     }

     LIST_CLASS(foo, struct foo, node, true)

   code corresponding to the following declaration is made available:

     struct foo_list;
     typedef struct foo_list foo_list_t;

     int foo_list_count (struct foo_list *list);

     struct foo *foo_list_head (struct foo_list *list);
     struct foo *foo_list_tail (struct foo_list *list);

     struct foo *foo_list_next (struct foo *item);
     struct foo *foo_list_prev (struct foo *item);

     void foo_list_push (struct foo_list *list, struct foo *object);
     void foo_list_enqueue (struct foo_list *list, struct foo *object);
     void foo_list_dequeue (struct foo_list *list, struct foo *object);
     void foo_list_unlink (struct foo_list *list, struct foo *object);

     void foo_list_move (struct foo_list *target, struct foo_list *source);
     void foo_list_join (struct foo_list *target, struct foo_list *source);
  */
/* Generates just the list type, not the methods.  */
#define LIST_CLASS_TYPE(name)						\
  struct name##_list							\
  {									\
    struct list list;							\
  };									\
  typedef struct name##_list name##_list_t;

#define LIST_CLASS_TYPE_need_type_true(name) LIST_CLASS_TYPE(name)
#define LIST_CLASS_TYPE_need_type_false(name)

/* If LIST_CLASS_TYPE(name) was used in scope, then pass false as the
   value of need_type, otherwise true.  */
#define LIST_CLASS(name, object_type, node_field, need_type)		\
  LIST_CLASS_TYPE_need_type_##need_type(name)				\
									\
  static inline void							\
  name##_list_init (struct name##_list *list, const char *name)		\
  {									\
    list_init (&list->list, name);					\
  }									\
									\
  static inline int							\
  name##_list_count (struct name##_list *list)				\
  {									\
    return list_count (&list->list);					\
  }									\
									\
  static inline object_type *						\
  name##_list_head (struct name##_list *list)				\
  {									\
    struct list_node *node = list_head (&list->list);			\
    if (! node)								\
      return NULL;							\
    return (void *) node - offsetof (object_type, node_field);		\
  }									\
									\
  static inline object_type *						\
  name##_list_tail (struct name##_list *list)				\
  {									\
    struct list_node *node = list_tail (&list->list);			\
    if (! node)								\
      return NULL;							\
    return (void *) node - offsetof (object_type, node_field);		\
  }									\
									\
  static inline object_type *						\
  name##_list_next (object_type *object)				\
  {									\
    assert (object);							\
    struct list_node *node = list_next (&object->node_field);		\
    if (! node)								\
      return NULL;							\
    return (void *) node - offsetof (object_type, node_field);		\
  }									\
									\
  static inline object_type *						\
  name##_list_prev (object_type *object)				\
  {									\
    assert (object);							\
    struct list_node *node = list_prev (&object->node_field);		\
    if (! node)								\
      return NULL;							\
    return (void *) node - offsetof (object_type, node_field);		\
  }									\
									\
  static inline void							\
  name##_list_push (struct name##_list *list, object_type *object)	\
  {									\
    list_push (&list->list, &object->node_field);			\
  }									\
									\
  static inline void							\
  name##_list_enqueue (struct name##_list *list, object_type *object)	\
  {									\
    list_enqueue (&list->list, &object->node_field);			\
  }									\
									\
  static inline object_type *						\
  name##_list_dequeue (struct name##_list *list)			\
  {									\
    struct list_node *node = list_dequeue (&list->list);		\
    if (! node)								\
      return NULL;							\
    return (void *) node - offsetof (object_type, node_field);		\
  }									\
									\
  static inline void							\
  name##_list_insert_after (struct name##_list *list,			\
			    object_type *item,				\
			    object_type *after)				\
  {									\
    list_insert_after (&list->list,					\
		       &item->node_field,				\
		       after ? &after->node_field : NULL);		\
  }									\
									\
  static inline void							\
  name##_list_unlink (struct name##_list *list, object_type *object)	\
  {									\
    list_unlink (&list->list, &object->node_field);			\
  }									\
									\
  static inline void							\
  name##_list_move (struct name##_list *target,				\
		    struct name##_list *source)				\
  {									\
    list_move (&target->list, &source->list);				\
  }									\
									\
  static inline void							\
  name##_list_join (struct name##_list *target,				\
		    struct name##_list *source)				\
  {									\
    list_join (&target->list, &source->list);				\
  }									\
  

#endif
