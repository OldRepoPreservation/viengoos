/* l4/bits/misc.h - L4 miscellaneous definitions for ia32.
   Copyright (C) 2003, 2004 Free Software Foundation, Inc.
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
# error "Never use <l4/bits/misc.h> directly; include <l4/misc.h> instead."
#endif


#define _L4_UNCACHEABLE_MEMORY		(_L4_WORD_C(1))
#define _L4_WRITE_COMBINING_MEMORY	(_L4_WORD_C(2))
#define _L4_WRITE_THROUGH_MEMORY	(_L4_WORD_C(5))
#define _L4_WRITE_PROTECTED_MEMORY	(_L4_WORD_C(6))
#define _L4_WRITE_BACK_MEMORY		(_L4_WORD_C(7))


/* Now incorporate the public interfaces the user has selected.  */
#ifdef _L4_INTERFACE_L4
#include <l4/bits/compat/misc.h>
#endif
#ifdef _L4_INTERFACE_GNU
#include <l4/bits/gnu/misc.h>
#endif
