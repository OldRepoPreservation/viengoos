/* assert.h - Assert declaration for libc-parts.
   Copyright (C) 2003, 2005, 2007, 2008 Free Software Foundation, Inc.
   Written by Neal H. Walfield <neal@gnu.org>.

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

#ifndef _ASSERT_H
#define _ASSERT_H 1

int printf (const char *fmt, ...);

#ifdef _L4_TEST_ENVIRONMENT
# include_next <assert.h>
# define assertx(__ax_expr, __ax_fmt, ...) \
  do					   \
    {					   \
      if (! (__ax_expr))		   \
	printf (__ax_fmt, ##__VA_ARGS__);  \
      assert (__ax_expr);		   \
    }					   \
  while (0)

#else

# ifndef NDEBUG
#include <l4/thread.h>

#  define assertx(__ax_expr, __ax_fmt, ...)				\
  do {									\
    extern const char program_name[];					\
    if (! (__ax_expr))							\
      {									\
	printf ("%s (%x):%s:%s:%d: %s failed",				\
		program_name, l4_myself (),				\
		__FILE__, __func__, __LINE__,				\
		#__ax_expr);						\
	if ((__ax_fmt) && *(__ax_fmt))					\
	  {								\
	    printf (": " __ax_fmt, ##__VA_ARGS__);			\
	  }								\
	printf ("\n");							\
									\
	extern int backtrace (void **array, int size);			\
									\
	void *a[10];							\
	int count = backtrace (a, sizeof (a) / sizeof (a[0]));		\
	int i;								\
	for (i = 0; i < count; i ++)					\
	  printf ("Backtrace: %p%s", a[i], i == count - 1 ? "" : " -> "); \
	printf ("\n");							\
									\
	for (;;);							\
      }									\
  } while (0)

#  define assert(__a_expr)				\
  assertx (__a_expr, "")

# else
#  define assert(expr) do { } while (0)
#  define assertx(expr, fmt, ...) do { } while (0)
# endif
# define assert_perror(err) assert(err == 0)

#endif /* _L4_TEST_ENVIRONMENT */

#endif /* _ASSERT_H */
