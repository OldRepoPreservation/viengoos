/* errno.h - Simpe errno declaration for libc-pars.
   Copyright (C) 2003 Free Software Foundation, Inc.
   Written by Marcus Brinkmann.

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

#ifndef _ERRNO_H
#define _ERRNO_H

/* This file is only used by Viengoos.  It's operation depends on the
   error codes and error_t.  However, Viengoos also uses errno, but
   only for strtol, etc. (and only during startup).  Thus, this
   definition suffices.  */

#if defined(_ENABLE_TESTS)

# undef _ERRNO_H
# include_next <errno.h>

/* We must never include the following file, as it will provoke clashes with
   the system's <errno.h> just included.  */
# define HURD_ERROR_H

#else

#include <hurd/error.h>

extern int errno;

#endif

typedef int error_t;

#endif
