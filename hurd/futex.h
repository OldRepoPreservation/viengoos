/* futex.h - Futex definitions.
   Copyright (C) 2008 Free Software Foundation, Inc.
   Written by Neal H. Walfield <neal@gnu.org>.

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

#ifndef _HURD_FUTEX_H
#define _HURD_FUTEX_H 1

#include <errno.h>
#include <hurd/addr.h>
#include <stdbool.h>
#include <time.h>

/* The interface to the kernel futex implementation.  This is only
   here because glibc really wants futexes.  If this project gets
   sufficient momentum, the kernel futex implementation should be
   replaced with a more microkernel friendly approach to locks.  */

enum
  {
    RM_futex = 800,
  };

#define RPC_STUB_PREFIX rm
#define RPC_ID_PREFIX RM
#undef RPC_TARGET_NEED_ARG
#define RPC_TARGET \
  ({ \
    extern struct hurd_startup_data *__hurd_startup_data; \
    __hurd_startup_data->rm; \
  })

#include <hurd/rpc.h>

/* Operations.  */
enum
  {
    FUTEX_WAIT,
    FUTEX_WAKE,
    FUTEX_WAKE_OP,
    FUTEX_CMP_REQUEUE,
#if 0
    /* We don't support these operations.  The first is deprecated and
       the second requires FDs which the kernel doesn't support.
       Although we could return EOPNOTSUPP, commenting them out
       catches any uses at compile-time.  */
    FUTEX_REQUEUE,
    FUTEX_FD,
#endif
  };

enum
  {
    FUTEX_OP_SET = 0,
    FUTEX_OP_ADD = 1,
    FUTEX_OP_OR = 2,
    FUTEX_OP_ANDN = 3,
    FUTEX_OP_XOR = 4
  };

enum
  {
    FUTEX_OP_CMP_EQ = 0,
    FUTEX_OP_CMP_NE = 1,
    FUTEX_OP_CMP_LT = 2,
    FUTEX_OP_CMP_LE = 3,
    FUTEX_OP_CMP_GT = 4,
    FUTEX_OP_CMP_GE = 5
  };

union futex_val2
{
  struct timespec timespec;
  int value;
};

union futex_val3
{
  int value;
  struct
  {
    int cmparg: 12;
    int oparg: 12;
    int cmp: 4;
    int op: 4;
  };
};

RPC (futex, 8, 1,
     addr_t, principal,
     void *, addr1, int, op, int, val1,
     bool, timeout, union futex_val2, val2,
     void *, addr2, union futex_val3, val3,
     /* Out: */
     long, out);

#undef RPC_STUB_PREFIX
#undef RPC_ID_PREFIX
#undef RPC_TARGET

#ifndef RM_INTERN
/* Standard futex signatures.  See futex documentation, e.g., Futexes
   are Tricky by Ulrich Drepper.  */
static inline long
futex (void *addr1, int op, int val1, struct timespec *timespec,
       void *addr2, int val3)
{
  error_t err;
  long ret;

  union futex_val2 val2;
  if (timespec)
    val2.timespec = *timespec;
  else
    __builtin_memset (&val2, 0, sizeof (val2));

  err = rm_futex (ADDR_VOID,
		  addr1, op, val1, !! timespec, val2, addr2,
		  (union futex_val3) val3, &ret);
  if (err)
    panic ("rm_futex returned %d", err);

  return ret;
}

/* If *F is VAL, wait until woken.  */
static inline long
futex_wait (int *f, int val)
{
  return futex (f, FUTEX_WAIT, val, NULL, 0, 0);
}

/* Signal NWAKE waiters waiting on futex F.  */
static inline long
futex_wake (int *f, int nwake)
{
  return futex (f, FUTEX_WAKE, nwake, NULL, 0, 0);
}
#endif /* !RM_INTERN */

#endif
