/* l4/gnu/misc.h - Public GNU interface for L4 miscellaneous functions.
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

#ifndef _L4_MISC_H
# error "Never use <l4/gnu/misc.h> directly; include <l4/misc.h> instead."
#endif

static inline l4_word_t
_L4_attribute_always_inline
l4_processor_control (l4_word_t proc, l4_word_t internal_freq,
		      l4_word_t external_freq, l4_word_t voltage)
{
  return _L4_processor_control (proc, internal_freq, external_freq,
				voltage);
}



static inline l4_word_t
_L4_attribute_always_inline
l4_memory_control (l4_word_t control, l4_word_t *attributes)
{
  return _L4_memory_control (control, attributes);
}


#define L4_DEFAULT_MEMORY	_L4_DEFAULT_MEMORY

static inline l4_word_t
_L4_attribute_always_inline
l4_set_page_attribute (l4_fpage_t fpage, l4_word_t attribute)
{
  return _L4_set_page_attribute (fpage, attribute);
}


static inline l4_word_t
_L4_attribute_always_inline
l4_set_pages_attributes (l4_word_t nr, l4_fpage_t *fpages,
			 l4_word_t *attributes)
{
  return _L4_set_pages_attributes (nr, fpages, attributes);
}
