/* physmem.c - Generic definitions.
   Copyright (C) 2003, 2005 Free Software Foundation, Inc.
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

#include <errno.h>
#include <l4.h>
#include <hurd/btree.h>
#include <hurd/cap-server.h>

#include <compiler.h>

#include "output.h"


/* The program name.  */
extern char program_name[];

#define BUG_ADDRESS	"<bug-hurd@gnu.org>"

int main (int argc, char *argv[]);


/* The following function must be defined by the architecture
   dependent code.  */

/* Switch execution transparently to thread TO.  The thread FROM,
   which must be the current thread, will be halted.  */
void switch_thread (l4_thread_id_t from, l4_thread_id_t to);


/* Return true if INDEX lies within (START, START+SIZE-1)
   inclusive.  */
static bool
within (l4_word_t index, l4_word_t start, l4_word_t size)
{
  return index >= start && index < start + size;
}

/* Return true if (INDEX1, INDEX1+SIZE1-1) inclusive overlaps with
   (INDEX2, INDEX2+SIZE2-1) inclusive.  */
static bool
overlap (l4_word_t index1, l4_word_t size1, l4_word_t index2, l4_word_t size2)
{
  return
    /* Is the start of the first region within the second?
          2  1  2  1
       or 2  1  1  2  */
    within (index1, index2, size2)
    /* Is the end of the first region within the second?
          1  2  1  2
       or 2  1  1  2  */
    || within (index1 + size1 - 1, index2, size2)
    /* Is start of the second region within the first?
          1  2  1  2
       or 1  2  2  1 */
    || within (index2, index1, size1);

  /* We have implicitly checked if the end of the second region is
     within the first (i.e. within (index2 + size2 - 1, index1, size1))
          2  1  2  1
       or 1  2  2  1
     in check 1 and check 3.  */
}

/* A region of memory.  */
struct region
{
  /* Start of the region.  */
  uintptr_t start;
  /* And its extent.  */
  size_t size;
};

static inline int
region_compare (const struct region *a, const struct region *b)
{
  if (overlap (a->start, a->size, b->start, b->size))
    return 0;
  else
    return a->start - b->start;
}

/* Forward.  */
struct frame_entry;

/* A frame referrs directly to physical memory.  Any allocated memory
   (for users) will have exactly one frame structure referring to
   it.  */
struct frame
{
  /* One reference per frame entry plus any active users.  */
  int refs;

  /* The physical memory allocated to this frame.  This is allocated
     lazily.  If this address portion is NULL, no memory has yet been
     allocated.  */
  l4_fpage_t memory;

  /* If a mapping has been made since the last time this frame was
     unmapped.  This does not mean that it actually is mapped as any
     users could have unmapped it themselves.  */
  bool may_be_mapped;

  /* List of frame entries referring to this frame.  */
  struct frame_entry *frame_entries;
};

/* Regions in containers refer to physical memory.  Multiple regions
   may refer to the same phsyical memory (thereby allowing sharing and
   COW).  Every region has its own frame entry which contains the
   per-region state.  FRAME refers to the physical memory.  */
struct frame_entry
{
  /* The name of this region within the containing container.  */
  struct region region;
  /* The physical memory backing the region.  */
  struct frame *frame;
  /* The frame entry may not reference all of the physical memory in
     FRAME (due to partial sharing, etc).  This offset to the start of
     the memory which this frame entry uses.  */
  size_t frame_offset;

  hurd_btree_node_t node;

  /* The list entry for FRAME's list of frame entries referring to
     itself.  */
  struct frame_entry *next;
  struct frame_entry **prevp;
};

BTREE_CLASS(frame_entry, struct frame_entry, struct region, region,
	    node, region_compare)

struct container
{
  pthread_mutex_t lock;
  /* List of allocate frames in this container.  */
  hurd_btree_frame_entry_t frame_entries;
};

/* Allocate an uninitialized frame entry structure.  Returns NULL if
   there is insufficient memory.  */
extern struct frame_entry *frame_entry_alloc (void);

/* Deallocate frame entry FRAME_ENTRY.  NB: this function does not
   deinitialize any resources FRAME_ENTRY may still reference.  It is
   the dual of frame_entry_alloc.  */
extern void frame_entry_dealloc (struct frame_entry *frame_entry);

/* Initialize the previously unitialized frame entry structure,
   FRAME_ENTRY to cover the region starting at byte START and
   extending SIZE bytes on container CONT.  SIZE must be a power of 2.
   CONT must be locked.  Physical memory is reserved, however, it is
   not allocated until a frame is attached and that frame is bound
   using frame_memory_bind.

   If the specified region overlaps with any in the container, EEXIST
   is returned.  */
extern error_t frame_entry_new (struct container *cont,
				struct frame_entry *frame_entry,
				uintptr_t start, size_t size);

/* Initialize the previously uninitialized frame entry structure
   FRAME_ENTRY to cover the region starting at byte START and
   extending SIZE bytes on container CONT.  If FRAME is non-NULL,
   FRAME_ENTRY refers to the physical memory in FRAME starting at
   offset OFFSET (hence when OFFSET is non-zero, FRAME_ENTRY only
   refers to a portion of FRAME).  SIZE must be a power of 2.  CONT
   must be locked.  A reference is added to FRAME.

   If the specified region overlaps with any in the container, EEXIST
   is returned.  */
extern error_t frame_entry_use (struct container *cont,
				struct frame_entry *frame_entry,
				uintptr_t start, size_t size,
				struct frame *frame,
				size_t offset);

/* Deinitialize frame entry FRAME_ENTRY which is in the locked
   container CONT dereferencing any frame it may be using in the
   process.  This does *not* deallocate the FRAME_ENTRY structure.
   This must still be done using frame_entry_dealloc.  */
extern void frame_entry_drop (struct container *cont,
			      struct frame_entry *frame_entry);

/* Find a frame entry which overlaps with the region START+SIZE and
   return it.  Returns NULL if no frame entry in container overlaps
   with the provided region.  CONT must be locked.  */
extern struct frame_entry *frame_entry_find (struct container *cont,
					     uintptr_t start,
					     size_t size);

/* Attach frame entry FRAME_ENTRY into container CONT.  FRAME_ENTRY
   must not currently be part of any container.  Returns EEXIST if
   FRAME_ENTRY overlaps with a frame entry in CONT.  */
extern error_t frame_entry_attach (struct container *cont,
				   struct frame_entry *frame_entry);

/* Detach frame entry FRAME_ENTRY from container CONT.  */
extern void frame_entry_detach (struct container *cont,
				struct frame_entry *frame_entry);

/* Allocate a frame structure holding SIZE bytes.  Physical memory
   must have already been reserved (by, e.g. a prior frame_entry_alloc
   call).  Allocation of the physical memory is deferred until
   frame_memory_alloc is called.  The returned frame has a single
   reference and is locked.  */
extern struct frame *frame_alloc (size_t size);

/* Bind frame FRAME to physical memory if not already done.  */
static inline void
frame_memory_bind (struct frame *frame)
{
  /* Actually allocates the physical memory.  */
  extern void frame_memory_alloc (struct frame *frame);

  if (! l4_address (frame->memory))
    frame_memory_alloc (frame);
}

/* Add a reference to frame FRAME.  */
static inline void
frame_ref (struct frame *frame)
{
  frame->refs ++;
}

/* Release a reference to frame FRAME.  FRAME must be locked.  When
   the last reference is removed, any exant client mappings will be
   unmapped, any physical memory will be deallocated and FRAME will be
   freed.  */
extern void frame_deref (struct frame *frame);

/* Add FRAME_ENTRY as a user of FRAME.  A reference for FRAME_ENTRY
   must already be held.  */
extern void frame_use (struct frame *frame,
		       struct frame_entry *frame_entry);

/* Remove FRAME_ENTRY as a user of FRAME.  A reference to frame is
   released in the process.  */
extern void frame_drop (struct frame *frame,
			struct frame_entry *frame_entry);

/* Allocate a new container object covering the NR_FPAGES fpages
   listed in FPAGES.  The object returned is locked and has one
   reference.  */
extern error_t container_alloc (l4_word_t nr_fpages, l4_word_t *fpages,
				struct container **r_container);

