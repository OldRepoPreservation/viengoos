/* l4/compat/misc.h - Public interface for L4 miscellaneous functions.
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
# error "Never use <l4/compat/misc.h> directly; include <l4/misc.h> instead."
#endif


/* 6.3 ProcessorControl [Privileged Systemcall]  */

/* Generic Programming Interface.  */
static inline L4_Word_t
_L4_attribute_always_inline
L4_ProcessorControl (L4_Word_t ProcessorNo, L4_Word_t InternalFrequency,
		     L4_Word_t ExternalFrequency, L4_Word_t voltage)
{
  return _L4_processor_control (ProcessorNo, InternalFrequency,
				ExternalFrequency, voltage);
}


/* 6.4 MemoryControl [Privileged Systemcall]  */

/* Generic Programming Interface.  */
static inline L4_Word_t
_L4_attribute_always_inline
L4_MemoryControl (L4_Word_t control, L4_Word_t *attributes)
{
  return _L4_memory_control (control, attributes);
}


#define L4_DefaultMemory	_L4_DEFAULT_MEMORY


/* Convenience Programming Interface.  */

static inline L4_Word_t
_L4_attribute_always_inline
L4_Set_PageAttribute (L4_Fpage_t f, L4_Word_t attribute)
{
  return _L4_set_page_attribute (f.raw, attribute);
}


static inline L4_Word_t
_L4_attribute_always_inline
L4_Set_PagesAttributes (L4_Word_t n, L4_Fpage_t *f, L4_Word_t *attributes)
{
  return _L4_set_pages_attributes (n, &f->raw, attributes);
}
