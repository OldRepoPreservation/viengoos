/* misc.h - L4 miscellaneous definitions for powerpc.
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

#ifndef _L4_MISC_H
# error "Never use <l4/bits/misc.h> directly; include <l4/misc.h> instead."
#endif

#define l4_write_through_memory		1
#define l4_write_back_memory		2
#define l4_cache_inhibited_memory	3
#define l4_cache_enabled_memory		4
#define l4_global_memory		5
#define l4_local_memory			6
#define l4_guarded_memory		7
#define l4_speculative_memory		8
