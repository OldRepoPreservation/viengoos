/* Client code for sigma0.
   Copyright (C) 2003 Free Software Foundation, Inc.
   This file is part of the GNU Hurd.

   The GNU Hurd is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.

   The GNU Hurd is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with the GNU C Library; if not, write to the Free
   Software Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
   02111-1307 USA.  */

#if HAVE_CONFIG_H
#include <config.h>
#endif

#include <l4.h>

/* Set the verbosity level in sigma0.  The only levels used currently
   are 1 to 3.  Returns 0 on success, otherwise an IPC error code.  */
void sigma0_set_verbosity (l4_word_t level);


/* Request a memory dump from sigma0.  If WAIT is true, wait until the
   dump is completed before continuing.  */
void sigma0_dump_memory (int wait);


/* Request the fpage FPAGE from sigma0.  */
void sigma0_get_fpage (l4_fpage_t fpage);


/* Request an fpage of the size 2^SIZE from sigma0.  The fpage will be
   fullly accessible.  */
l4_fpage_t sigma0_get_any (unsigned int size);
