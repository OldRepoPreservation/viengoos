/* space.h - Public interface to L4 spaces.
   Copyright (C) 2003 Free Software Foundation, Inc.
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
#define _L4_SPACE_H	1

#include <l4/types.h>
#include <l4/math.h>
#include <l4/bits/space.h>
#include <l4/syscall.h>

/* fpage support.  */
#define l4_no_access		0x00
#define l4_executable		0x01
#define l4_writable		0x02
#define l4_readable		0x04
#define l4_fully_accessible	(l4_readable | l4_writable | l4_executable)
#define l4_read_exec_only	(l4_readable | l4_executable)

/* FIXME: These are compound statements and can not be used for
   initialization of global variables in C99.  */
#define l4_nilpage ((l4_fpage_t) { .raw = 0 })
/* FIXME: When gcc supports unnamed fields in initializer.  */
#define l4_complete_address_space \
  ((l4_fpage_t) { .page.rights = 0, .page.log2_size = 1, .page.base = 0 })


static inline l4_word_t
__attribute__((__always_inline__))
l4_is_nil_fpage (l4_fpage_t fpage)
{
  return fpage.raw == l4_nilpage.raw;
}


static inline l4_fpage_t
__attribute__((__always_inline__))
l4_fpage (l4_word_t base, int size)
{
  l4_fpage_t fpage;
  l4_word_t msb = l4_msb (size) - 1;

  fpage.base = base >> 10;
  fpage.log2_size = size ? ((1 << msb) == size ? msb : msb + 1) : 0;
  fpage.rights = l4_no_access;

  return fpage;
}


static inline l4_fpage_t
__attribute__((__always_inline__))
l4_fpage_log2 (l4_word_t base, int log2_size)
{
  l4_fpage_t fpage;

  fpage.base = base >> 10;
  fpage.log2_size = log2_size;
  fpage.rights = l4_no_access;
  return fpage;
}


static inline l4_word_t
__attribute__((__always_inline__))
l4_address (l4_fpage_t fpage)
{
  return fpage.base << 10;
}


static inline l4_word_t
__attribute__((__always_inline__))
l4_size (l4_fpage_t fpage)
{
  return 1 << fpage.log2_size;
}


static inline l4_word_t
__attribute__((__always_inline__))
l4_size_log2 (l4_fpage_t fpage)
{
  return fpage.log2_size;
}


static inline l4_word_t
__attribute__((__always_inline__))
l4_rights (l4_fpage_t fpage)
{
  return fpage.rights;
}


static inline void
__attribute__((__always_inline__))
l4_set_rights (l4_fpage_t *fpage, l4_word_t rights)
{
  fpage->rights = rights;
}


static inline l4_fpage_t
__attribute__((__always_inline__))
l4_fpage_add_rights (l4_fpage_t fpage, l4_word_t rights)
{
  l4_fpage_t new_fpage = fpage;
  new_fpage.rights |= rights;
  return new_fpage;
}


static inline void
__attribute__((__always_inline__))
l4_fpage_add_rights_to (l4_fpage_t *fpage, l4_word_t rights)
{
  fpage->rights |= rights;
}


static inline l4_fpage_t
__attribute__((__always_inline__))
l4_fpage_remove_rights (l4_fpage_t fpage, l4_word_t rights)
{
  l4_fpage_t new_fpage = fpage;
  new_fpage.rights &= ~rights;
  return new_fpage;
}


static inline void
__attribute__((__always_inline__))
l4_fpage_remove_rights_from (l4_fpage_t *fpage, l4_word_t rights)
{
  fpage->rights &= ~rights;
}


/* l4_unmap convenience interface.  */

static inline void
__attribute__((__always_inline__))
l4_unmap_fpage (l4_fpage_t fpage)
{
  l4_load_mr (0, fpage.raw);
  l4_unmap (0);
  l4_store_mr (0, &fpage.raw);
}


static inline void
__attribute__((__always_inline__))
l4_unmap_fpages (l4_word_t nr, l4_fpage_t *fpages)
{
  l4_load_mrs (0, nr, (l4_word_t *) fpages);
  l4_unmap ((nr - 1) & L4_UNMAP_COUNT_MASK);
  l4_store_mrs (0, nr, (l4_word_t *) fpages);
}


static inline void
__attribute__((__always_inline__))
l4_flush (l4_fpage_t fpage)
{
  l4_load_mr (0, fpage.raw);
  l4_unmap (L4_UNMAP_FLUSH);
  l4_store_mr (0, &fpage.raw);
}


static inline void
__attribute__((__always_inline__))
l4_flush_fpages (l4_word_t nr, l4_fpage_t *fpages)
{
  l4_load_mrs (0, nr, (l4_word_t *) fpages);
  l4_unmap (L4_UNMAP_FLUSH | ((nr - 1) & L4_UNMAP_COUNT_MASK));
  l4_store_mrs (0, nr, (l4_word_t *) fpages);
}


static inline l4_fpage_t
__attribute__((__always_inline__))
l4_get_status (l4_fpage_t fpage)
{
  l4_fpage_t save_fpage;
  l4_fpage_t status;
  
  save_fpage = l4_fpage_remove_rights (fpage, l4_fully_accessible);
  l4_load_mr (0, save_fpage.raw);
  l4_unmap (0);
  l4_store_mr (0, &status.raw);
  return status;
}


static inline l4_word_t
__attribute__((__always_inline__))
l4_was_referenced (l4_fpage_t fpage)
{
  return fpage.referenced;
}


static inline l4_word_t
__attribute__((__always_inline__))
l4_was_written (l4_fpage_t fpage)
{
  return fpage.written;
}


static inline l4_word_t
__attribute__((__always_inline__))
l4_was_executed (l4_fpage_t fpage)
{
  return fpage.executed;
}

#endif	/* l4/syscall.h */
