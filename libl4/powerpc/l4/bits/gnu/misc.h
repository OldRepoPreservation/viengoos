/* l4/bits/gnu/misc.h - GNU L4 miscellaneous features for powerpc.
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
# error "Never use <l4/bits/gnu/misc.h> directly; include <l4/misc.h> instead."
#endif


#ifdef __cplusplus
extern "C" {
#if 0 /* Just to make Emacs auto-indent happy.  */
}
#endif
#endif /* __cplusplus */


#define L4_WRITE_THROUGH_MEMORY		_L4_WRITE_THROUGH_MEMORY
#define L4_WRITE_BACK_MEMORY		_L4_WRITE_BACK_MEMORY
#define L4_CACHE_INHIBITED_MEMORY	_L4_CACHE_INHIBITED_MEMORY
#define L4_CACHE_ENABLED_MEMORY		_L4_CACHE_ENABLED_MEMORY
#define L4_GLOBAL_MEMORY		_L4_GLOBAL_MEMORY
#define L4_LOCAL_MEMORY			_L4_LOCAL_MEMORY
#define L4_GUARDED_MEMORY		_L4_GUARDED_MEMORY
#define L4_SPECULATIVE_MEMORY		_L4_SPECULATIVE_MEMORY


#ifdef __cplusplus
}
#endif
