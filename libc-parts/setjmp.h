/* setjmp.h - setjmp and longjmp declarations.
   Copyright (C) 2008 Free Software Foundation, Inc.
   Written by Neal H. Walfield.

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

#ifndef _SETJMP_H
#define _SETJMP_H	1

#ifdef RM_INTERN

#include <stdint.h>

struct jmp_buf
{
#if __i386__
  /* See ia32-setjmp.S.  */
  uintptr_t registers[6];
#endif
};
typedef struct jmp_buf jmp_buf[1];

void _longjmp(jmp_buf env, int val) __attribute__ ((noreturn));
int _setjmp(jmp_buf env) __attribute__ ((returns_twice));

#else

#include_next <setjmp.h>

#endif

#endif
