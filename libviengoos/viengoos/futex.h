/* vg_futex.h - Futex definitions.
   Copyright (C) 2008 Free Software Foundation, Inc.
   Written by Neal H. Walfield <neal@gnu.org>.

   GNU Hurd is free software: you can redistribute it and/or modify it
   under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   GNU Hurd is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with GNU Hurd.  If not, see
   <http://www.gnu.org/licenses/>.  */

#ifndef _HURD_FUTEX_H
#define _HURD_FUTEX_H 1

#include <viengoos/addr.h>
#include <hurd/startup.h>
#include <hurd/error.h>
#include <stdbool.h>
#define __need_timespec
#include <time.h>

/* The interface to the kernel futex implementation.  This is only
   here because glibc really wants futexes.  If this project gets
   sufficient momentum, the kernel futex implementation should be
   replaced with a more microkernel friendly approach to locks.  */

enum
  {
    VG_futex = 800,
  };

#define RPC_STUB_PREFIX vg
#define RPC_ID_PREFIX VG

#include <viengoos/rpc.h>

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
#define FUTEX_OP_CLEAR_WAKE_IF_GT_ONE \
  (union futex_val3) { { 1, 0, FUTEX_OP_CMP_GT, FUTEX_OP_SET } }

RPC (futex, 7, 1, 0,
     /* cap_t principal, cap_t thread, */
     void *, addr1, int, op, int, val1,
     bool, timeout, union futex_val2, val2,
     void *, addr2, union futex_val3, val3,
     /* Out: */
     long, out);

#undef RPC_STUB_PREFIX
#undef RPC_ID_PREFIX

#ifndef RM_INTERN
#include <errno.h>

struct vg_futex_return
{
  error_t err;
  long ret;
};

static inline struct vg_futex_return
__attribute__((always_inline))
futex_using (struct hurd_message_buffer *mb,
	     void *addr1, int op, int val1, struct timespec *timespec,
	     void *addr2, int val3)
{
  union futex_val2 val2;
  if (timespec)
    val2.timespec = *timespec;
  else
    __builtin_memset (&val2, 0, sizeof (val2));

  error_t err;
  long ret = 0; /* Elide gcc warning.  */
  if (mb)
    err = vg_futex_using (mb,
			  VG_ADDR_VOID, VG_ADDR_VOID,
			  addr1, op, val1, !! timespec, val2, addr2,
			  (union futex_val3) val3, &ret);
  else
    err = vg_futex (VG_ADDR_VOID, VG_ADDR_VOID,
		    addr1, op, val1, !! timespec, val2, addr2,
		    (union futex_val3) val3, &ret);
  return (struct vg_futex_return) { err, ret };
}

/* Standard futex signatures.  See futex documentation, e.g., Futexes
   are Tricky by Ulrich Drepper.  */
static inline struct vg_futex_return
__attribute__((always_inline))
futex (void *addr1, int op, int val1, struct timespec *timespec,
       void *addr2, int val3)
{
  return futex_using (NULL, addr1, op, val1, timespec, addr2, val3);
}


/* If *F is VAL, wait until woken.  */
static inline long
__attribute__((always_inline))
futex_wait_using (struct hurd_message_buffer *mb, int *f, int val)
{
  struct vg_futex_return ret;
  ret = futex_using (mb, f, FUTEX_WAIT, val, NULL, 0, 0);
  if (ret.err)
    {
      errno = ret.err;
      return -1;
    }
  return ret.ret;
}

static inline long
__attribute__((always_inline))
futex_wait (int *f, int val)
{
  return futex_wait_using (NULL, f, val);
}


/* If *F is VAL, wait until woken.  */
static inline long
__attribute__((always_inline))
futex_timed_wait (int *f, int val, struct timespec *timespec)
{
  struct vg_futex_return ret;
  ret = futex (f, FUTEX_WAIT, val, timespec, 0, 0);
  if (ret.err)
    {
      errno = ret.err;
      return -1;
    }
  return ret.ret;
}


/* Signal NWAKE waiters waiting on vg_futex F.  */
static inline long
__attribute__((always_inline))
futex_wake_using (struct hurd_message_buffer *mb, int *f, int nwake)
{
  struct vg_futex_return ret;
  ret = futex_using (mb, f, FUTEX_WAKE, nwake, NULL, 0, 0);
  if (ret.err)
    {
      errno = ret.err;
      return -1;
    }
  return ret.ret;
}

static inline long
__attribute__((always_inline))
futex_wake (int *f, int nwake)
{
  return futex_wake_using (NULL, f, nwake);
}
#endif /* !RM_INTERN */

#endif
