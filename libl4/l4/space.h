/* l4/space.h - Public interface to L4 spaces.
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


typedef _L4_RAW
(_L4_word_t, _L4_STRUCT3
 ({
   _L4_BITFIELD3
     (_L4_word_t,
      _L4_BITFIELD (rights, 4),
      _L4_BITFIELD (log2_size, 6),
      _L4_BITFIELD_32_64 (base, 22, 54));
 },
 {
   /* Alias names for RIGHTS.  */
   _L4_BITFIELD3
     (_L4_word_t,
      _L4_BITFIELD (executable, 1),
      _L4_BITFIELD (writable, 1),
      _L4_BITFIELD (readable, 1));
 },
 {
   /* Names for status bits as returned from l4_unmap.  */
   _L4_BITFIELD3
     (_L4_word_t,
      _L4_BITFIELD (executed, 1),
      _L4_BITFIELD (written, 1),
      _L4_BITFIELD (referenced, 1));
 })) __L4_fpage_t;

/* fpage support.  */
#define _L4_no_access		0x00
#define _L4_executable		0x01
#define _L4_writable		0x02
#define _L4_readable		0x04
#define _L4_fully_accessible	(_L4_readable | _L4_writable | _L4_executable)
#define _L4_read_exec_only	(_L4_readable | _L4_executable)

#define _L4_nilpage			((_L4_fpage_t) 0)
#define _L4_complete_address_space	((_L4_fpage_t) (1 << 4))


static inline _L4_word_t
_L4_attribute_always_inline
_L4_is_nil_fpage (_L4_fpage_t fpage)
{
  return fpage == _L4_nilpage;
}


static inline _L4_fpage_t
_L4_attribute_always_inline
_L4_fpage (_L4_word_t base, int size)
{
  __L4_fpage_t fpage;
  _L4_word_t msb = _L4_msb (size) - 1;

  fpage.base = base >> 10;
  fpage.log2_size = size ? ((1 << msb) == size ? msb : msb + 1) : 0;
  fpage.rights = _L4_no_access;

  return fpage.raw;
}


static inline _L4_fpage_t
_L4_attribute_always_inline
_L4_fpage_log2 (_L4_word_t base, int log2_size)
{
  __L4_fpage_t fpage;

  fpage.base = base >> 10;
  fpage.log2_size = log2_size;
  fpage.rights = _L4_no_access;

  return fpage.raw;
}


static inline _L4_word_t
_L4_attribute_always_inline
_L4_address (_L4_fpage_t fpage)
{
  __L4_fpage_t _fpage;

  _fpage.raw = fpage;
  return _fpage.base << 10;
}


static inline _L4_word_t
_L4_attribute_always_inline
_L4_size (_L4_fpage_t fpage)
{
  __L4_fpage_t _fpage;

  _fpage.raw = fpage;
  return 1 << _fpage.log2_size;
}


static inline _L4_word_t
_L4_attribute_always_inline
_L4_size_log2 (_L4_fpage_t fpage)
{
  __L4_fpage_t _fpage;

  _fpage.raw = fpage;
  return _fpage.log2_size;
}


static inline _L4_word_t
_L4_attribute_always_inline
_L4_rights (_L4_fpage_t fpage)
{
  __L4_fpage_t _fpage;

  _fpage.raw = fpage;
  return _fpage.rights;
}


static inline void
_L4_attribute_always_inline
_L4_set_rights (_L4_fpage_t *fpage, _L4_word_t rights)
{
  __L4_fpage_t _fpage;

  _fpage.raw = *fpage;
  _fpage.rights = rights;
  *fpage = _fpage.raw;
}


static inline _L4_fpage_t
_L4_attribute_always_inline
_L4_fpage_add_rights (_L4_fpage_t fpage, _L4_word_t rights)
{
  __L4_fpage_t _fpage;

  _fpage.raw = fpage;
  _fpage.rights |= rights;
  return _fpage.raw;
}


static inline void
_L4_attribute_always_inline
_L4_fpage_add_rights_to (_L4_fpage_t *fpage, _L4_word_t rights)
{
  __L4_fpage_t _fpage;

  _fpage.raw = *fpage;
  _fpage.rights |= rights;
  *fpage = _fpage.raw;
}


static inline _L4_fpage_t
_L4_attribute_always_inline
_L4_fpage_remove_rights (_L4_fpage_t fpage, _L4_word_t rights)
{
  __L4_fpage_t _fpage;

  _fpage.raw = fpage;
  _fpage.rights &= ~rights;
  return _fpage.raw;
}


static inline void
_L4_attribute_always_inline
_L4_fpage_remove_rights_from (_L4_fpage_t *fpage, _L4_word_t rights)
{
  __L4_fpage_t _fpage;

  _fpage.raw = *fpage;
  _fpage.rights &= ~rights;
  *fpage = _fpage.raw;
}


/* l4_unmap convenience interface.  */

static inline void
_L4_attribute_always_inline
_L4_unmap_fpage (_L4_fpage_t fpage)
{
  _L4_load_mr (0, fpage);
  _L4_unmap (0);
  _L4_store_mr (0, &fpage);
}


static inline void
_L4_attribute_always_inline
_L4_unmap_fpages (_L4_word_t nr, _L4_fpage_t *fpages)
{
  _L4_load_mrs (0, nr, fpages);
  _L4_unmap ((nr - 1) & _L4_UNMAP_COUNT_MASK);
  _L4_store_mrs (0, nr, fpages);
}


static inline void
_L4_attribute_always_inline
_L4_flush (_L4_fpage_t fpage)
{
  _L4_load_mr (0, fpage);
  _L4_unmap (_L4_UNMAP_FLUSH);
  _L4_store_mr (0, &fpage);
}


static inline void
_L4_attribute_always_inline
_L4_flush_fpages (_L4_word_t nr, _L4_fpage_t *fpages)
{
  _L4_load_mrs (0, nr, fpages);
  _L4_unmap (_L4_UNMAP_FLUSH | ((nr - 1) & _L4_UNMAP_COUNT_MASK));
  _L4_store_mrs (0, nr, fpages);
}


static inline _L4_fpage_t
_L4_attribute_always_inline
_L4_get_status (_L4_fpage_t fpage)
{
  _L4_fpage_remove_rights_from (&fpage, _L4_fully_accessible);
  _L4_load_mr (0, fpage);
  _L4_unmap (0);
  _L4_store_mr (0, &fpage);
  return fpage;
}


static inline _L4_word_t
_L4_attribute_always_inline
_L4_was_referenced (_L4_fpage_t fpage)
{
  __L4_fpage_t _fpage;

  _fpage.raw = fpage;
  return _fpage.referenced;
}


static inline _L4_word_t
_L4_attribute_always_inline
_L4_was_written (_L4_fpage_t fpage)
{
  __L4_fpage_t _fpage;

  _fpage.raw = fpage;
  return _fpage.written;
}


static inline _L4_word_t
_L4_attribute_always_inline
_L4_was_executed (_L4_fpage_t fpage)
{
  __L4_fpage_t _fpage;

  _fpage.raw = fpage;
  return _fpage.executed;
}


/* Now incorporate the public interfaces the user has selected.  */
#ifdef _L4_INTERFACE_L4
#include <l4/compat/space.h>
#endif
#ifdef _L4_INTERFACE_GNU
#include <l4/gnu/space.h>
#endif

#endif	/* l4/space.h */
