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


#define L4_FPAGE_NO_ACCESS		_l4_no_access
#define L4_FPAGE_EXECUTABLE		_L4_executable
#define L4_FPAGE_WRITABLE		_L4_writable
#define L4_FPAGE_READABLE		_L4_readable
#define L4_FPAGE_FULLY_ACCESSIBLE	_L4_fully_accessible
#define L4_FPAGE_READ_EXEC_ONLY		_L4_read_exec_only

#define L4_NILPAGE			_L4_nilpage
#define L4_COMPLETE_ADDRESS_SPACE	_L4_complete_address_space


static inline l4_word_t
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


static inline l4_word_t
_L4_attribute_always_inline
l4_was_referenced (l4_fpage_t fpage)
{
  return _L4_was_referenced (fpage);
}


static inline l4_word_t
_L4_attribute_always_inline
l4_was_written (l4_fpage_t fpage)
{
  return _L4_was_written (fpage);
}


static inline l4_word_t
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
