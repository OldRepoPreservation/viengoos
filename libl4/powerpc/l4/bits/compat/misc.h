/* l4/bits/compat/misc.h - L4 miscellaneous features for powerpc.
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
# error "Never use <l4/bits/compat/misc.h> directly; include <l4/misc.h> instead."
#endif


#define L4_WriteThroughMemory		_L4_WRITE_THROUGH_MEMORY
#define L4_WriteBackMemory		_L4_WRITE_BACK_MEMORY
#define L4_CacheInhibitedMemory		_L4_CACHE_INHIBITED_MEMORY
#define L4_CacheEnabledMemory		_L4_CACHE_ENABLED_MEMORY
#define L4_GlobalMemory			_L4_GLOBAL_MEMORY
#define L4_LocalMemory			_L4_LOCAL_MEMORY
#define L4_GuardedMemory		_L4_GUARDED_MEMORY
#define L4_SpeculativeMemory		_L4_SPECULATIVE_MEMORY
