/* l4/bits/compat/ipc.h - L4 IPC features for ia32.
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

#ifndef _L4_IPC_H
# error "Never use <l4/bits/compat/ipc.h> directly; include <l4/ipc.h> instead."
#endif


#define L4_AllocateNewCacheLines	_L4_ALLOCATE_NEW_CACHE_LINES
#define L4_DoNotAllocateNewCacheLines	_L4_DO_NOT_ALLOCATE_NEW_CACHE_LINES
#define L4_AllocateOnlyNewCacheLines	_L4_ALLOCATE_ONLY_NEW_CACHE_LINES
