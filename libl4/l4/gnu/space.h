/* l4/gnu/space.h - Public GNU interface for L4 spaces.
   Copyright (C) 2004 Free Software Foundation, Inc.
   Written by Marcus Brinkmann <marcus@gnu.org>.

   This file is part of the GNU L4 library.
 
   The GNU L4 library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public License
   as published by the Free Software Foundation; either version 2.1 of
   the License, or (at your option) any later version.
 
   The GNU L4 library is distributed in the hope that it will be
   useful, but WITHOUT ANY WARRANTY; without even the implied warranty
   of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU Lesser General Public License for more details.
 
   You should have received a copy of the GNU Lesser General Public
   License along with the GNU L4 library; if not, write to the Free
   Software Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
   02111-1307, USA.  */

#ifndef _L4_SPACE_H
# error "Never use <l4/gnu/space.h> directly; include <l4/space.h> instead."
#endif


#define L4_FPAGE_NO_ACCESS		_L4_no_access
#define L4_FPAGE_EXECUTABLE		_L4_executable
#define L4_FPAGE_WRITABLE		_L4_writable
#define L4_FPAGE_READABLE		_L4_readable
#define L4_FPAGE_FULLY_ACCESSIBLE	_L4_fully_accessible
#define L4_FPAGE_READ_WRITE_ONLY	_L4_read_write_only
#define L4_FPAGE_READ_EXEC_ONLY		_L4_read_exec_only

#define L4_NILPAGE			_L4_nilpage
#define L4_COMPLETE_ADDRESS_SPACE	_L4_complete_address_space


static inline bool
_L4_attribute_always_inline
l4_is_nil_fpage (l4_fpage_t fpage)
{
  return _L4_is_nil_fpage (fpage);
}


static inline l4_fpage_t
_L4_attribute_always_inline
l4_fpage (l4_word_t base, int size)
{
  return _L4_fpage (base, size);
}


static inline l4_fpage_t
_L4_attribute_always_inline
l4_fpage_log2 (l4_word_t base, int log2_size)
{
  return _L4_fpage_log2 (base, log2_size);
}


static inline l4_word_t
_L4_attribute_always_inline
l4_address (l4_fpage_t fpage)
{
  return _L4_address (fpage);
}


static inline l4_word_t
_L4_attribute_always_inline
l4_size (l4_fpage_t fpage)
{
  return _L4_size (fpage);
}


static inline l4_word_t
_L4_attribute_always_inline
l4_size_log2 (l4_fpage_t fpage)
{
  return _L4_size_log2 (fpage);
}


static inline l4_word_t
_L4_attribute_always_inline
l4_rights (l4_fpage_t fpage)
{
  return _L4_rights (fpage);
}


static inline void
_L4_attribute_always_inline
l4_set_rights (l4_fpage_t *fpage, l4_word_t rights)
{
  _L4_set_rights (fpage, rights);
}


static inline l4_fpage_t
_L4_attribute_always_inline
l4_fpage_add_rights (l4_fpage_t fpage, l4_word_t rights)
{
  return _L4_fpage_add_rights (fpage, rights);
}


static inline void
_L4_attribute_always_inline
l4_fpage_add_rights_to (l4_fpage_t *fpage, l4_word_t rights)
{
  _L4_fpage_add_rights_to (fpage, rights);
}


static inline l4_fpage_t
_L4_attribute_always_inline
l4_fpage_remove_rights (l4_fpage_t fpage, l4_word_t rights)
{
  return _L4_fpage_remove_rights (fpage, rights);
}


static inline void
_L4_attribute_always_inline
l4_fpage_remove_rights_from (l4_fpage_t *fpage, l4_word_t rights)
{
  _L4_fpage_remove_rights_from (fpage, rights);
}


static inline l4_fpage_t
_L4_attribute_always_inline
l4_unmap_fpage (l4_fpage_t fpage)
{
  return _L4_unmap_fpage (fpage);
}


static inline void
_L4_attribute_always_inline
l4_unmap_fpages (l4_word_t nr, l4_fpage_t *fpages)
{
  _L4_unmap_fpages (nr, fpages);
}


static inline l4_fpage_t
_L4_attribute_always_inline
l4_flush (l4_fpage_t fpage)
{
  return _L4_flush (fpage);
}


static inline void
_L4_attribute_always_inline
l4_flush_fpages (l4_word_t nr, l4_fpage_t *fpages)
{
  _L4_flush_fpages (nr, fpages);
}


static inline l4_fpage_t
_L4_attribute_always_inline
l4_get_status (l4_fpage_t fpage)
{
  return _L4_get_status (fpage);
}


static inline bool
_L4_attribute_always_inline
l4_was_referenced (l4_fpage_t fpage)
{
  return _L4_was_referenced (fpage);
}


static inline bool
_L4_attribute_always_inline
l4_was_written (l4_fpage_t fpage)
{
  return _L4_was_written (fpage);
}


static inline bool
_L4_attribute_always_inline
l4_was_executed (l4_fpage_t fpage)
{
  return _L4_was_executed (fpage);
}


static inline l4_word_t
_L4_attribute_always_inline
l4_space_control (l4_thread_id_t space, l4_word_t control,
		  l4_fpage_t kip, l4_fpage_t utcb,
		  l4_thread_id_t redirector, l4_word_t *old_control)
{
  return _L4_space_control (space, control, kip, utcb, redirector,
			    old_control);
}


/* GNU extensions.  */

#include <l4/kip.h>

/* The maximum number of fpages required to cover a page aligned range
   of memory.  If 2^X is the minimum page size, and 2^Y is the size of
   the virtual address space, then this is 1 if X == Y and 2 * (Y - X
   - 1) otherwise.  Why this is the case is best illustrated by the
   following picture, which uses one character for the minimum page
   size, and 64 characters for the whole address space (for example, X
   == 10, Y == 16).  The second line indicates a range of memory, and
   the third line its separation in spanning fpages.

   0123456789012345678901234567890123456789012345678901234567890123
    --------------------------------------------------------------
    12233334444444455555555555555556666666666666666777777778888990

   Required fpages is thus 10 == 2 * (16 - 10 - 1).  */

#if _L4_WORDSIZE == 32
#define L4_FPAGE_SPAN_MAX	(2 * (32 - L4_MIN_PAGE_SIZE_LOG2 - 1))
#define _L4_MAX_PAGE_SIZE_LOG2	(32)
#else
#define L4_FPAGE_SPAN_MAX	(2 * (64 - L4_MIN_PAGE_SIZE_LOG2 - 1))
#define _L4_MAX_PAGE_SIZE_LOG2	(64)
#endif


/* Determine the fpages covering the (page aligned) virtual address
   space from START to END (inclusive).  START must be page aligned,
   while END must be the address of the last byte in the area.  FPAGES
   must be an array of at least

   L4_FPAGE_SPAN_MAX - (l4_min_page_size () - L4_MIN_PAGE_SIZE_LOG2)

   fpages (you can just use L4_FPAGE_SPAN_MAX if you need a constant
   expression).  The function returns the number of fpages returned in
   FPAGES.  The generated fpages are fully accessible.  */
static inline unsigned int
l4_fpage_span (l4_word_t start, l4_word_t end, l4_fpage_t *fpages)
{
  l4_word_t min_page_size = l4_min_page_size ();
  unsigned int nr_fpages = 0;

  if (start > end)
    return 0;

  /* Round START down to a multiple of the minimum page size.  */
  start &= ~(min_page_size - 1);

  /* Round END up to one less than a multiple of the minimum page size.  */
  end = (end & ~(min_page_size - 1)) + min_page_size - 1;

  end = ((end + min_page_size) & ~(min_page_size - 1)) - 1;

  /* END is now at least MIN_PAGE_SIZE - 1 larger than START.  */
  do
    {
      unsigned int addr_align;
      unsigned int size_align;

      /* Each fpage must be self-aligned.  */
      addr_align = start ? l4_lsb (start) - 1 : (_L4_MAX_PAGE_SIZE_LOG2 - 1);
      size_align = (end + 1 - start) ? l4_msb (end + 1 - start) - 1
	: (_L4_MAX_PAGE_SIZE_LOG2 - 1);
      if (addr_align < size_align)
	size_align = addr_align;

      fpages[nr_fpages]
	= l4_fpage_add_rights (l4_fpage_log2 (start, size_align),
			       L4_FPAGE_FULLY_ACCESSIBLE);

      /* This may overflow and result in zero.  In that case, the
	 while loop will terminate.  */
      start += l4_size (fpages[nr_fpages]);
      nr_fpages++;
    }
  while (start && start < end);

  return nr_fpages;
}


/* Determine the fpages covering the (page aligned) virtual address
   space from START to END (inclusive) under the conditions that it is
   going to be mapped (or granted) to the virtual address DEST.  START
   and DEST must be page aligned, while END must be the address of the
   last byte in the area.  MAX_FPAGES is the count of available fpages
   that can be stored at FPAGES.  The actual number required can be
   very large (thousands and tens of thousands) due to bad alignment.
   The function returns the number of fpages returned in FPAGES.  The
   generated fpages are fully accessible.  */
static inline unsigned int
l4_fpage_xspan (l4_word_t start, l4_word_t end, l4_word_t dest,
		l4_fpage_t *fpages, l4_word_t max_fpages)
{
  l4_word_t min_page_size = l4_min_page_size ();
  unsigned int nr_fpages = 0;

  if (start > end)
    return 0;

  /* Round START and DEST down to a multiple of the minimum page size.  */
  start &= ~(min_page_size - 1);
  dest &= ~(min_page_size - 1);

  /* Round END up to one less than a multiple of the minimum page size.  */
  end = (end & ~(min_page_size - 1)) + min_page_size - 1;

  end = ((end + min_page_size) & ~(min_page_size - 1)) - 1;

  /* END is now at least MIN_PAGE_SIZE - 1 larger than START.  */
  do
    {
      unsigned int addr_align;
      unsigned int dest_align;
      unsigned int size_align;

      /* Each fpage must be self-aligned.  */
      addr_align = start ? l4_lsb (start) - 1 : (_L4_MAX_PAGE_SIZE_LOG2 - 1);
      dest_align = dest ? l4_lsb (dest) - 1 : (_L4_MAX_PAGE_SIZE_LOG2 - 1);
      size_align = (end + 1 - start) ? l4_msb (end + 1 - start) - 1
	: (_L4_MAX_PAGE_SIZE_LOG2 - 1);

      if (addr_align < size_align)
	size_align = addr_align;
      if (dest_align < size_align)
	size_align = dest_align;

      fpages[nr_fpages]
	= l4_fpage_add_rights (l4_fpage_log2 (start, size_align),
			       L4_FPAGE_FULLY_ACCESSIBLE);

      /* This may overflow and result in zero.  In that case, the
	 while loop will terminate.  */
      start += l4_size (fpages[nr_fpages]);
      dest += l4_size (fpages[nr_fpages]);
      nr_fpages++;
    }
  while (start && start < end && nr_fpages < max_fpages);

  return nr_fpages;
}
