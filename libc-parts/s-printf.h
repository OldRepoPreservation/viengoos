/* s-printf.h - Simple printf interface.
   Copyright (C) 2008 Free Software Foundation, Inc.
   Written by Neal H. Walfield <neal@gnu.org>.

   This file is part of the GNU Hurd.

   The GNU Hurd is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 3 of the
   License, or (at your option) any later version.

   The GNU Hurd is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see
   <http://www.gnu.org/licenses/>.  */

#ifndef S_PRINTF_H
#define S_PRINTF_H

#include <stdarg.h>

/* The simple printf routines are just that, simple.  They are
   implemented in terms of putchar (or the callback) meaning that they
   can be slow if putchar is expensive.  They only support a limited
   number of output conversions, namely those for strings, integers
   and characters.  Notably missing are the floating-point related
   output conversions.  However, they use very limited memory, in
   particular, they don't use malloc.  This makes them ideal for use
   during early initialization and in fragile situations (e.g.,
   asserts and panics).  */


extern int s_puts (const char *str);

extern int s_cputs (int (*putchar) (int), const char *str);


extern int s_printf (const char *fmt, ...);

extern int s_cprintf (int (*putchar) (int), const char *fmt, ...);


extern int s_vprintf (const char *fmt, va_list ap);

extern int s_cvprintf (int (*putchar) (int), const char *fmt, va_list ap);

#endif
