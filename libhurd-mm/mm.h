/* mm.h - Memory management interface.
   Copyright (C) 2004, 2005 Free Software Foundation, Inc.
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

#ifndef HURD_MM_MM_H
#define HURD_MM_MM_H

#include <stdint.h>
#include <sys/types.h>
#include <l4/types.h>
#include <hurd/physmem.h>

/* Start the memory manager including starting the user pager and the
   virtual memory management subsystem on thread PAGER_TID.  */
extern void hurd_mm_init (l4_thread_id_t pager_tid);

/* A region of memory.  */
struct region
{
  /* Start of the region.  */
  uintptr_t start;
  /* And its extent.  */
  size_t size;
};

/* Backing store abstraction.  */
struct hurd_store;
typedef struct hurd_store hurd_store_t;

/* Physical memory abstraction.  */
struct hurd_memory;
typedef struct hurd_memory hurd_memory_t;

/* Allocate a new memory structure referring to size bytes of physical
   memory.  Return NULL on failure.  The returned memory needs to be
   integrated into the appropriate store's cache (using
   hurd_memory_insert).  */
extern hurd_memory_t *hurd_memory_alloc (size_t size);

/* Allocate a new memory structure referring to the physical memory on
   container CONTAINER starting at CONT_START and continuing for SIZE
   bytes.  The physical memory becomes fully managed by the memory
   manager and must only be deallocated via the hurd_memory_dealloc
   interface.  (Hence, CONTAINER must not be deallocated until the
   memory manager no longer references it.) */
extern hurd_memory_t *hurd_memory_use (hurd_pm_container_t container,
				       uintptr_t cont_start,
				       size_t size);

/* Allocate a new memory structure referring to the physical memory on
   container CONTAINER starting at CONT_START and continuing for SIZE
   bytes.  The physical memory is logically copied (and deallocated
   from CONTAINER if MOVE is true) to an internal container.  Hence,
   unlike hurd_memory_use, hurd_memory_transfer obtains its own
   reference to the physical memory.  NB: if MOVE is false the memory
   is referenced by two containers and those count doubly against the
   task's physical memory allocation.  */
extern hurd_memory_t *hurd_memory_transfer (hurd_pm_container_t container,
					    uintptr_t cont_start,
					    size_t size,
					    bool move);

/* Deallocate the physical memory MEMORY associated with store STORE
   starting at START (relative to the base of MEMORY) and continuing
   for LENGHT bytes.  If START is 0 and LENGTH is the length of
   MEMORY, MEMORY is deallocated (and detached from STORE).  If START
   is greater than zero and START + LENGTH is less than the size of
   MEMORY then MEMORY is adjusted to refer to the first portion of
   memory, a new MEMORY structure is allocated to refer the end and
   physical memory referred to by the middle part is freed.
   Otherwise, MEMORY is adjusted to refer to the physical memory which
   will not be deallocated and the rest of the physical memory is
   deallocated.  */
extern void hurd_memory_dealloc (hurd_store_t *store, hurd_memory_t *memory,
				 uintptr_t start, size_t length);

/* Install a map of memory MEMORY starting at offset MEMORY_OFFSET
   (relative to the base of MEMORY) extending LENGTH bytes in the
   address space at virtual memory address VADDR.  (To map an entire
   memory block, pass OFFSET equal to 0 and LENGTH equal to
   MEMORY->STORE.SIZE.)  OFFSET must be aligned on a multiple of the
   base page size and LENGTH must be a multiple of the base page
   size.  */
extern error_t hurd_memory_map (hurd_memory_t *memory, size_t memory_offset,
				size_t length, uintptr_t vaddr);

/* Thread THREAD faulted at address ADDR which is contained in the
   registered region REGION.  */
typedef void (*hurd_store_fault_t) (hurd_store_t *store, void *hook,
				    l4_thread_id_t thread,
				    struct region vm_region,
				    off64_t store_offset,
				    uintptr_t addr, l4_word_t access);

/* Initialize a store.  STORE points to allocated memory.  */
extern void hurd_store_init (hurd_store_t *store,
			     void *hook, hurd_store_fault_t fault);

/* The number of bytes required for a store.  */
extern size_t hurd_store_size (void) __attribute__ ((const));

extern void hurd_store_destroy (hurd_store_t *store);

/* Either allocate a region exactly at the specified address or
   fail.  */
#define HURD_VM_HERE (1 << 0)

/* A store manager may call this to indicate that STORE will manage
   faults for the addresses starting at ADDRESS and extending for SIZE
   bytes.  If ADDRESS is 0, the memory manager selects a free virtual
   memory region.  If ADDRESS is non-zero and HURD_VM_HERE is not set
   in FLAGS, then ADDRESS is used as a hint.  If HURD_VM_HERE is set
   in the flags, ADDRESS is used if possible otherwise, EEXIST is
   returned.  The function returns the start of the virtual memory
   address actually used on success in *USED_ADDRESS if USED_ADDRESS
   is not-NULL.  On success, 0 is returned.  An error code
   otherwise.  */
extern error_t hurd_store_bind_to_vm (hurd_store_t *store,
				      off64_t store_offset,
				      uintptr_t address, size_t size,
				      uintptr_t flags, int map_now,
				      uintptr_t *used_address);

/* Record that memory MEMORY caches backing store STORE starting at
   byte STORE_START.  */
extern void hurd_store_cache (hurd_store_t *store, uintptr_t store_start,
			      hurd_memory_t *memory);

/* Find the first memory region (i.e. the one with the lowest starting
   address) which intersects with regions covering STORE starting at
   STORE_START and spanning LENGTH bytes.  If none is found, return
   NULL.

   NB: The intersection of any two memory regions will always be NULL,
   however, it is possible that the region one is looking up on the
   store is covered by multiple memory regions.  This function returns
   the first of those.  */
extern hurd_memory_t *hurd_store_find_cached (hurd_store_t *store,
					      uintptr_t store_start,
					      size_t length);

/* Flush any memory mapping the backing store starting at STORE_START
   and continuing for LENGTH bytes to backing store.  */
extern void hurd_store_flush (hurd_store_t *store,
			      uintptr_t store_start, size_t length);

#endif /* HURD_MM_MM_H */
