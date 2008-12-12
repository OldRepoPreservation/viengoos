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

#ifndef __have_vg_thread_id_t
# define __have_vg_thread_id_t

# ifdef USE_L4
#  include <l4.h>
typedef l4_thread_id_t vg_thread_id_t;
#  define vg_niltid l4_nilthread
#  define VG_THREAD_ID_FMT "%x"
# else
#  include <stdint.h>
typedef uint64_t vg_thread_id_t;
#  define vg_niltid -1
#  define VG_THREAD_ID_FMT "%llx"
# endif

#endif /* !__have_vg_thread_id_t */

#ifndef __have_activation_frame
# define __have_activation_frame

# include <stdint.h>

# ifdef USE_L4
#  include <l4/ipc.h>
# endif

struct hurd_message_buffer;

struct activation_frame
{
  /* **** ia32-exception-entry.S silently depends on the layout of
     this structure up to and including the next field ****  */
#ifdef i386
  union
  {
    uintptr_t regs[10];
    struct
    {
      uintptr_t eax;
      uintptr_t ecx;
      uintptr_t edx;
      uintptr_t eflags;
      uintptr_t eip;
      uintptr_t ebx;
      uintptr_t edi;
      uintptr_t esi;
      uintptr_t ebp;
      uintptr_t esp;
    };
  };
#else
# error Not ported to this architecture!
#endif
  /* The base of the stack to use when running
     hurd_activation_handler_normal.  If NULL, then the interrupted
     stack is used.  */
  void *normal_mode_stack;

  struct activation_frame *next;
  struct activation_frame *prev;

  struct hurd_message_buffer *message_buffer;

#ifdef USE_L4
  /* We need to save parts of the UTCB.  */
  l4_word_t saved_sender;
  l4_word_t saved_receiver;
  l4_word_t saved_timeout;
  l4_word_t saved_error_code;
  l4_word_t saved_flags;
  l4_word_t saved_br0;
  l4_msg_t saved_message;
#endif

#define ACTIVATION_FRAME_CANARY 0x10ADAB1E
  uintptr_t canary;
};
#endif

#if defined(__need_vg_thread_id_t) || defined (__need_activation_frame)
# undef __need_vg_thread_id_t
# undef __need_activation_frame
#else

#ifndef _HURD_THREAD_H
#define _HURD_THREAD_H 1

#include <stdint.h>
#include <hurd/types.h>
#include <hurd/addr.h>
#include <hurd/addr-trans.h>
#include <hurd/cap.h>
#include <hurd/messenger.h>
#include <setjmp.h>

/* Cause the activation frame to assume the state of the long jump
   buffer BUF.  If SET_RET is true, the normal function return value
   is set to RET.  */
extern void hurd_activation_frame_longjmp (struct activation_frame *af,
					   jmp_buf buf,
					   bool set_ret, int ret);

struct hurd_fault_catcher
{
#define HURD_FAULT_CATCHER_MAGIC 0xb01dface
  uintptr_t magic;

  /* Start of the region to watch.  */
  uintptr_t start;
  /* Length of the region in bytes.  */
  uintptr_t len;
  /* The callback.  Return true to continue execute.  False to throw a
     SIGSEGV.  */
  bool (*callback) (struct activation_frame *activation_frame,
		    uintptr_t fault);

  struct hurd_fault_catcher *next;
  struct hurd_fault_catcher **prevp;
};

/* Register a fatch catch handler.  */
extern void hurd_fault_catcher_register (struct hurd_fault_catcher *catcher);

/* Unregister a fault catch handler.  */
extern void hurd_fault_catcher_unregister (struct hurd_fault_catcher *catcher);

/* The user thread control block.  */
struct vg_utcb
{
  union
  {
    /* The following structures are examined or modified by the
       kernel.  */

    /* **** ia32-exception-entry.S silently depends on the layout of
       this structure ****  */
    struct
    {
      union
      {
	struct
	{
	  /* Whether the thread is in activated mode.  If so, any
	     activations that arrive during this time will be queued
	     or dropped.  */
	  uintptr_t activated_mode : 1;
	  /* Set by the kernel to indicated that there is a pending
	     message.  */
	  uintptr_t pending_message : 1;
	  /* Set by the kernel to indicate whether the thread was
	     interrupted while the EIP is in the transition range.  */
	  uintptr_t interrupt_in_transition : 1;
	};
	uintptr_t mode;
      };

      /* The value of the IP and SP when the thread was running.  */
      uintptr_t saved_ip;
      uintptr_t saved_sp;
      /* The state of the thread (as returned by _L4_exchange_regs)  */
      uintptr_t saved_thread_state;

      /* Top of the activation frame stack (i.e., the active
	 activation).  */
      struct activation_frame *activation_stack;
      /* The bottom of the activation stack.  */
      struct activation_frame *activation_stack_bottom;

      uintptr_t activation_handler_sp;
      uintptr_t activation_handler_ip;
      uintptr_t activation_handler_end;

      /* The protected payload of the capability that invoked the
	 messenger that caused this activation.  */
      uint64_t protected_payload;
      /* The messenger's id.  */
      uint64_t messenger_id;

      uintptr_t inline_words[VG_MESSENGER_INLINE_WORDS];
      addr_t inline_caps[VG_MESSENGER_INLINE_CAPS];

      union
      {
	struct
	{
	  int inline_word_count : 2;
	  int inline_cap_count : 1;
	};
	int inline_data : 3;
      };

      /* The following fields are not examined or modified by the
	 kernel.  */

      /* The CRC protects the above fields by checking for
	 modification, which can happen if a call back function uses
	 too much stack.  The fields following crc are not protected
	 by the crc as they are expected to be changed by the
	 activation handler.  */

      uintptr_t crc;

      /* The exception buffer.  */
      struct hurd_message_buffer *exception_buffer;
      /* The current extant IPC.  */
      struct hurd_message_buffer *extant_message;

      struct hurd_fault_catcher *catchers;

      /* The alternate activation stack.  */
      void *alternate_stack;
      bool alternate_stack_inuse;

#define UTCB_CANARY0 0xCA17A1
#define UTCB_CANARY1 0xDEADB15D
      uintptr_t canary0;
      uintptr_t canary1;
    };
    char data[PAGESIZE];
  };
};

/* A thread object's user accessible capability slots.  */
enum
  {
    /* Root of the address space.  */
    THREAD_ASPACE_SLOT = 0,
    /* The activity the thread is bound to.  */
    THREAD_ACTIVITY_SLOT = 1,
    /* The messenger to post exceptions to.  */
    THREAD_EXCEPTION_MESSENGER = 2,
    /* The user thread control block.  Must be a cap_page.  */
    THREAD_UTCB = 3,

    /* Total number of capability slots in a thread object.  This must
       be a power of 2.  */
    THREAD_SLOTS = 4,
  };
#define THREAD_SLOTS_LOG2 2

enum
{
  HURD_EXREGS_SET_UTCB = 0x2000,
  HURD_EXREGS_SET_EXCEPTION_MESSENGER = 0x1000,
  HURD_EXREGS_SET_ASPACE = 0x800,
  HURD_EXREGS_SET_ACTIVITY = 0x400,
  HURD_EXREGS_SET_SP = _L4_XCHG_REGS_SET_SP,
  HURD_EXREGS_SET_IP = _L4_XCHG_REGS_SET_IP,
  HURD_EXREGS_SET_SP_IP = _L4_XCHG_REGS_SET_SP | _L4_XCHG_REGS_SET_IP,
  HURD_EXREGS_SET_EFLAGS = _L4_XCHG_REGS_SET_FLAGS,
  HURD_EXREGS_SET_USER_HANDLE = _L4_XCHG_REGS_SET_USER_HANDLE,
  HURD_EXREGS_SET_REGS = (HURD_EXREGS_SET_UTCB
			  | HURD_EXREGS_SET_EXCEPTION_MESSENGER
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

enum
  {
    RM_thread_exregs = 600,
    RM_thread_id,
    RM_thread_activation_collect,
  };

#ifdef RM_INTERN
struct thread;
typedef struct thread *thread_t;
#else
typedef addr_t thread_t;
#endif

#define RPC_STUB_PREFIX rm
#define RPC_ID_PREFIX RM

#include <hurd/rpc.h>

struct hurd_thread_exregs_in
{
  uintptr_t aspace_cap_properties_flags;
  struct cap_properties aspace_cap_properties;

  uintptr_t sp;
  uintptr_t ip;
  uintptr_t eflags;
  uintptr_t user_handle;
};

struct hurd_thread_exregs_out
{
  uintptr_t sp;
  uintptr_t ip;
  uintptr_t eflags;
  uintptr_t user_handle;
};

/* l4_exregs wrapper.  */
RPC (thread_exregs, 6, 1, 4,
     /* cap_t principal, cap_t thread, */
     uintptr_t, control, struct hurd_thread_exregs_in, in,
     cap_t, aspace, cap_t, activity, cap_t, utcb, cap_t, exception_messenger,
     /* Out: */
     struct hurd_thread_exregs_out, out,
     cap_t, aspace_out, cap_t, activity_out, cap_t, utcb_out,
     cap_t, exception_messenger_out)

static inline error_t
thread_start (addr_t thread)
{
  struct hurd_thread_exregs_in in;
  struct hurd_thread_exregs_out out;

  return rm_thread_exregs (ADDR_VOID, thread,
			   HURD_EXREGS_START | HURD_EXREGS_ABORT_IPC,
			   in, ADDR_VOID, ADDR_VOID, ADDR_VOID, ADDR_VOID,
			   &out, NULL, NULL, NULL, NULL);
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
			   in, ADDR_VOID, ADDR_VOID, ADDR_VOID, ADDR_VOID,
			   &out, NULL, NULL, NULL, NULL);
}

static inline error_t
thread_stop (addr_t thread)
{
  struct hurd_thread_exregs_in in;
  struct hurd_thread_exregs_out out;

  return rm_thread_exregs (ADDR_VOID, thread,
			   HURD_EXREGS_STOP | HURD_EXREGS_ABORT_IPC,
			   in, ADDR_VOID, ADDR_VOID, ADDR_VOID, ADDR_VOID,
			   &out, NULL, NULL, NULL, NULL);
}

/* Return the unique integer associated with thread THREAD.  */
RPC(thread_id, 0, 1, 0,
    /* cap_t, principal, cap_t, thread, */
    vg_thread_id_t, tid)

/* Cause the delivery of a pending message, if any.  */
RPC(thread_activation_collect, 0, 0, 0
    /* cap_t principal, cap_t thread */)

#undef RPC_STUB_PREFIX
#undef RPC_ID_PREFIX

static inline vg_thread_id_t
vg_myself (void)
{
  vg_thread_id_t tid;
  error_t err = rm_thread_id (ADDR_VOID, ADDR_VOID, &tid);
  if (err)
    return vg_niltid;
  return tid;
}

#endif /* _HURD_THREAD_H */
#endif /* __need_vg_thread_id_t */
