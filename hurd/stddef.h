/* hurd/stddef.h - Standard definitions for the GNU Hurd.
   Copyright (C) 2007, 2008 Free Software Foundation, Inc.
   Written by Neal H. Walfield <neal@gnu.org>

   This file is part of the GNU Hurd.

   GNU Hurd is free software: you can redistribute it and/or modify it
   under the terms of the GNU Lesser General Public License as
   published by the Free Software Foundation, either version 3 of the
   License, or (at your option) any later version.

   GNU Hurd is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with GNU Hurd.  If not, see
   <http://www.gnu.org/licenses/>.  */

#ifndef _HURD_STDDEF_H
#define _HURD_STDDEF_H	1

#include <l4/types.h>
#include <assert.h>

/* Convenient debugging macros.  */
#ifdef DEBUG_ELIDE

# define do_debug(level) if (0)

#else

# ifndef DEBUG_COND
/* The default DEBUG_COND is LEVEL <= output_debug.  */
extern int output_debug;
#  define DEBUG_COND(level) ((level) <= output_debug)
# endif

# define do_debug(level) if (DEBUG_COND(level))

#endif /* DEBUG_ELIDE */

#include <l4/thread.h>

/* Print a debug message if DEBUG_COND is true.  */
#define debug(level, fmt, ...)					\
  do								\
    {								\
      extern const char program_name[];				\
      do_debug (level)						\
        printf ("%s (%x):%s:%d: " fmt "\n",			\
		program_name, l4_myself (), __func__, __LINE__,	\
	        ##__VA_ARGS__);					\
    }								\
  while (0)

/* Print an error message and fail.  This function must be provided by
   the run-time.  */
extern void __attribute__ ((__noreturn__))
  panic_ (const char *func, int line, const char *fmt, ...);

#define panic(fmt, args...)			\
  panic_(__func__, __LINE__, fmt, ##args)

/* XXX: We define these here as they are useful macros, everyone uses
   them and everyone includes this header file.  We should put them
   somewhere else.  */
#if i386
#define PAGESIZE 0x1000U
#define PAGESIZE_LOG2 12U
#else
#error Not ported to this architecture.
#endif

/* Linux compatible names.  */
#define PAGE_SIZE PAGESIZE
#define PAGE_SHIFT PAGESIZE_LOG2
#define PAGE_MASK (~(PAGE_SIZE-1))

#define likely(expr) __builtin_expect ((expr), 1)
#define unlikely(expr) __builtin_expect ((expr), 0)

#endif	/* _HURD_STDDEF_H */
