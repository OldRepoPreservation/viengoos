/* thread.h - Thread definitions.
   Copyright (C) 2007, 2008 Free Software Foundation, Inc.
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

#ifndef _HURD_THREAD_H
#define _HURD_THREAD_H 1

#include <hurd/types.h>
#include <hurd/addr-trans.h>
#include <hurd/cap.h>
#include <hurd/startup.h>
#include <l4/syscall.h>
#include <l4/ipc.h>

enum
  {
    RM_thread_exregs = 600,
    RM_thread_wait_object_destroyed,
    RM_thread_raise_exception,
  };

#ifdef RM_INTERN
struct thread;
typedef struct thread *thread_t;
#else
typedef addr_t thread_t;
#endif

struct exception_frame
{
#if i386
  /* eax, ecx, edx, eflags, eip, ebx, edi, esi.  */
  l4_word_t regs[8];
#else
# error Not ported to this architecture!
#endif
  struct exception_frame *next;
  struct exception_frame *prev;
  l4_msg_t exception;

  /* We need to save parts of the UTCB.  */
  l4_word_t saved_sender;
  l4_word_t saved_receiver;
  l4_word_t saved_timeout;
  l4_word_t saved_error_code;
  l4_word_t saved_flags;
  l4_word_t saved_br0;
  l4_msg_t saved_message;
};

struct exception_page
{
  union
  {
    /* Whether the thread is in activation mode or not.  If so, no
       exception will be delivered.

       **** ia32-exception-entry.S silently depends on the layout of
       this structure ****  */
    struct
    {
      union
      {
	struct
	{
	  /* Whether the thread is in activated mode.  */
	  l4_word_t activated_mode : 1;
	  /* Set by the kernel to indicated that there is a pending
	     message.  */
	  l4_word_t pending_message : 1;
	  /* Set by the kernel to indicate whether the thread was
	     interrupted while the EIP is in the transition range.  */
	  l4_word_t interrupt_in_transition : 1;
	};
	l4_word_t mode;
      };

      /* The value of the IP and SP when the thread was running.  */
      l4_word_t saved_ip;
      l4_word_t saved_sp;
      /* The state of the thread (as returned by _L4_exchange_regs)  */
      l4_word_t saved_thread_state;

      /* Top of the exception frame stack.  */
      struct exception_frame *exception_stack;
      /* Bottom of the exception frame stack.  */
      struct exception_frame *exception_stack_bottom;

      l4_word_t exception_handler_sp;
      l4_word_t exception_handler_ip;
      l4_word_t exception_handler_end;

      /* The exception.  */
      l4_msg_t exception;

      l4_word_t crc;
    };
    char data[PAGESIZE];
  };
};

enum
  {
    /* Root of the address space.  */
    THREAD_ASPACE_SLOT = 0,
    /* The activity the thread is bound to.  */
    THREAD_ACTIVITY_SLOT = 1,
    /* Where exceptions are saved.  Must be a cap_page.  */
    THREAD_EXCEPTION_PAGE_SLOT = 2,
  };

enum
{
  HURD_EXREGS_SET_EXCEPTION_PAGE = 0x1000,

  HURD_EXREGS_SET_ASPACE = 0x800,
  HURD_EXREGS_SET_ACTIVITY = 0x400,
  HURD_EXREGS_SET_SP = _L4_XCHG_REGS_SET_SP,
  HURD_EXREGS_SET_IP = _L4_XCHG_REGS_SET_IP,
  HURD_EXREGS_SET_SP_IP = _L4_XCHG_REGS_SET_SP | _L4_XCHG_REGS_SET_IP,
  HURD_EXREGS_SET_EFLAGS = _L4_XCHG_REGS_SET_FLAGS,
  HURD_EXREGS_SET_USER_HANDLE = _L4_XCHG_REGS_SET_USER_HANDLE,
  HURD_EXREGS_SET_REGS = (HURD_EXREGS_SET_EXCEPTION_PAGE
			  | HURD_EXREGS_SET_ASPACE
			  | HURD_EXREGS_SET_ACTIVITY
			  | HURD_EXREGS_SET_SP
			  | HURD_EXREGS_SET_IP
			  | HURD_EXREGS_SET_EFLAGS
			  | HURD_EXREGS_SET_USER_HANDLE),

  HURD_EXREGS_GET_REGS = _L4_XCHG_REGS_DELIVER,

  HURD_EXREGS_START = _L4_XCHG_REGS_SET_HALT,
  HURD_EXREGS_STOP = _L4_XCHG_REGS_SET_HALT | _L4_XCHG_REGS_HALT,

  HURD_EXREGS_ABORT_SEND = _L4_XCHG_REGS_CANCEL_SEND,
  HURD_EXREGS_ABORT_RECEIVE = _L4_XCHG_REGS_CANCEL_RECV,
  HURD_EXREGS_ABORT_IPC = HURD_EXREGS_ABORT_SEND | _L4_XCHG_REGS_CANCEL_RECV,
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

struct hurd_thread_exregs_in
{
  addr_t aspace;
  uintptr_t aspace_cap_properties_flags;
  struct cap_properties aspace_cap_properties;

  addr_t activity;

  addr_t exception_page;

  l4_word_t sp;
  l4_word_t ip;
  l4_word_t eflags;
  l4_word_t user_handle;

  addr_t aspace_out;
  addr_t activity_out;
  addr_t exception_page_out;
};

struct hurd_thread_exregs_out
{
  l4_word_t sp;
  l4_word_t ip;
  l4_word_t eflags;
  l4_word_t user_handle;
};

/* l4_exregs wrapper.  */
RPC (thread_exregs, 4, 1,
     addr_t, principal,
     addr_t, thread,
     l4_word_t, control,
     struct hurd_thread_exregs_in, in,
     /* Out: */
     struct hurd_thread_exregs_out, out)

static inline error_t
thread_start (addr_t thread)
{
  struct hurd_thread_exregs_in in;
  struct hurd_thread_exregs_out out;

  return rm_thread_exregs (ADDR_VOID, thread,
			   HURD_EXREGS_START | HURD_EXREGS_ABORT_IPC,
			   in, &out);
}

static inline error_t
thread_start_sp_ip (addr_t thread, uintptr_t sp, uintptr_t ip)
{
  struct hurd_thread_exregs_in in;
  struct hurd_thread_exregs_out out;

  in.sp = sp;
  in.ip = ip;

  return rm_thread_exregs (ADDR_VOID, thread,
			   HURD_EXREGS_START | HURD_EXREGS_ABORT_IPC
			   | HURD_EXREGS_SET_SP_IP,
			   in, &out);
}

static inline error_t
thread_stop (addr_t thread)
{
  struct hurd_thread_exregs_in in;
  struct hurd_thread_exregs_out out;

  return rm_thread_exregs (ADDR_VOID, thread,
			   HURD_EXREGS_STOP | HURD_EXREGS_ABORT_IPC,
			   in, &out);
}

/* Cause the caller to wait until OBJECT is destroyed.  Returns the
   object's return code in RETURN_CODE.  */
RPC(thread_wait_object_destroyed, 2, 1,
    addr_t, principal, addr_t, object,
    /* Out: */
    uintptr_t, return_code);

/* The kernel interprets the payload as a short l4_msg_t buffer: that
   is one that does not exceed 128 bytes.  */
struct exception_buffer
{
  char payload[128];
};

RPC(thread_raise_exception, 3, 0,
    addr_t, principal, addr_t, thread,
    struct exception_buffer, exception_buffer)

#undef RPC_STUB_PREFIX
#undef RPC_ID_PREFIX
#undef RPC_TARGET

#endif
