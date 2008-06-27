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

#ifndef ASM

#include <l4/types.h>
#include <assert.h>

/* Safe printf and putchar routines.  In particular, they don't call malloc and
   work in place.  This makes it appropriate for situations in which
   malloc is not yet working and when the state is suspected to be
   compromised (e.g., assert or panic).  */
#ifndef S_PRINTF
# if defined(RM_INTERN) || defined(_L4_TEST_ENVIRONMENT) || defined(_ENABLE_TESTS)
#  define S_PRINTF printf
# else
#  define S_PRINTF s_printf
# endif
#endif
extern int S_PRINTF (const char *fmt, ...);
#ifndef S_PUTCHAR
# if defined(RM_INTERN) || defined(_L4_TEST_ENVIRONMENT) || defined(_ENABLE_TESTS)
#  define S_PUTCHAR putchar
# else
#  define S_PUTCHAR s_putchar
# endif
#endif
extern int S_PUTCHAR (int chr);

/* Convenient debugging macros.  */
#define DEBUG_BOLD(text) "\033[01;31m" text "\033[00m"

#if defined(DEBUG_ELIDE)
# if DEBUG_ELIDE + 0 == 0
#  define do_debug(level) if (0)
# endif
#endif

#ifndef do_debug
# ifndef DEBUG_COND
extern int output_debug;
#  ifdef DEBUG_ELIDE
/* We elide some code at compile time.  */
#   define DEBUG_COND(level) \
  ((level) <= output_debug && (level) < DEBUG_ELIDE)
#  else
/* The default DEBUG_COND is LEVEL <= output_debug.  */
#   define DEBUG_COND(level) ((level) <= output_debug)
#  endif /* DEBUG_ELIDE.  */
# endif /* DEBUG_COND.  */

# define do_debug(level) if (DEBUG_COND(level))

#endif /* do_debug.  */

#include <l4/thread.h>

/* Print a debug message if DEBUG_COND is true.  */
#define debug(level, fmt, ...)						\
  do									\
    {									\
      extern char *program_name;					\
      extern int S_PRINTF (const char *, ...);				\
      do_debug (level)							\
        S_PRINTF ("%s (%x):%s:%d: " fmt "\n",				\
		   program_name, l4_myself (), __func__, __LINE__,	\
		   ##__VA_ARGS__);					\
    }									\
  while (0)

/* Print an error message and fail.  This function must be provided by
   the run-time.  */
extern void __attribute__ ((__noreturn__, format (printf, 3, 4)))
  panic_ (const char *func, int line, const char *fmt, ...);

#define panic(fmt, args...)			\
  panic_(__func__, __LINE__, fmt, ##args)

#endif /* ! ASM  */

/* XXX: We define these here as they are useful macros, everyone uses
   them and everyone includes this header file.  We should put them
   somewhere else.  */
#if i386
#define PAGESIZE 0x1000
#define PAGESIZE_LOG2 12
#else
#error Not ported to this architecture.
#endif

/* Linux compatible names.  */
#define PAGE_SIZE PAGESIZE
#define PAGE_SHIFT PAGESIZE_LOG2
#define PAGE_MASK (~(PAGE_SIZE-1))

#ifndef ASM
#define likely(expr) __builtin_expect ((expr), 1)
#define unlikely(expr) __builtin_expect ((expr), 0)
#endif

#endif	/* _HURD_STDDEF_H */
