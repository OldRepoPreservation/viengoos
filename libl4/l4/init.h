/* init.h - Public interface to L4 for initialization.
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

#ifndef _L4_INIT_H
#define _L4_INIT_H	1

#include <l4/kip.h>


/* Initialize the global data.  */
static inline void
__attribute__((__always_inline__))
l4_init (void)
{
  l4_api_version_t version;
  l4_api_flags_t flags;
  l4_kernel_id_t id;

  __l4_kip = l4_kernel_interface (&version, &flags, &id);
};

#endif	/* l4/init.h */
