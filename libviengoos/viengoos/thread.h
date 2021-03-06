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

#if defined(__need_vg_thread_id_t)
# undef __need_vg_thread_id_t
#else

#ifndef _VIENGOOS_THREAD_H
#define _VIENGOOS_THREAD_H 1

#include <stdint.h>
#include <viengoos/addr.h>
#include <viengoos/cap.h>
#include <viengoos/messenger.h>

/* The user thread control block.  */
struct vg_utcb
{
  /* Generic data.  */
  struct
  {
    union
    {
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

	uintptr_t activation_handler_sp;
	uintptr_t activation_handler_ip;
	uintptr_t activation_handler_end;

	/* The protected payload of the capability that invoked the
	   messenger that caused this activation.  */
	uint64_t protected_payload;
	/* The messenger's id.  */
	uint64_t messenger_id;

	uintptr_t inline_words[VG_MESSENGER_INLINE_WORDS];
	vg_addr_t inline_caps[VG_MESSENGER_INLINE_CAPS];

	union
	{
	  struct
	  {
	    int inline_word_count : 2;
	    int inline_cap_count : 1;
	  };
	  int inline_data : 3;
	};
      };

      char data[256];
    };
  };

  /* Architecture-specific data.  */
  struct
  {
    union
    {
      struct
      {
      };

      char data[256];
    };
  };
};

/* A thread object's user accessible capability slots.  */
enum
  {
    /* Root of the address space.  */
    VG_THREAD_ASPACE_SLOT = 0,
    /* The activity the thread is bound to.  */
    VG_THREAD_ACTIVITY_SLOT = 1,
    /* The messenger to post exceptions to.  */
    VG_THREAD_EXCEPTION_MESSENGER = 2,
    /* The user thread control block.  Must be a vg_cap_page.  */
    VG_THREAD_UTCB = 3,

    /* Total number of capability slots in a thread object.  This must
       be a power of 2.  */
    VG_THREAD_SLOTS = 4,
  };
#define VG_THREAD_SLOTS_LOG2 2

enum
  {
    VG_thread_exregs = 600,
    VG_thread_id,
    VG_thread_activation_collect,
  };

#ifdef RM_INTERN
struct thread;
typedef struct thread *vg_thread_t;
#else
typedef vg_addr_t vg_thread_t;
#endif

#define RPC_STUB_PREFIX vg
#define RPC_ID_PREFIX VG

#include <viengoos/rpc.h>

enum
{
  VG_EXREGS_SET_UTCB = 0x2000,
  VG_EXREGS_SET_EXCEPTION_MESSENGER = 0x1000,
  VG_EXREGS_SET_ASPACE = 0x800,
  VG_EXREGS_SET_ACTIVITY = 0x400,
  VG_EXREGS_SET_SP = _L4_XCHG_REGS_SET_SP,
  VG_EXREGS_SET_IP = _L4_XCHG_REGS_SET_IP,
  VG_EXREGS_SET_SP_IP = _L4_XCHG_REGS_SET_SP | _L4_XCHG_REGS_SET_IP,
  VG_EXREGS_SET_EFLAGS = _L4_XCHG_REGS_SET_FLAGS,
  VG_EXREGS_SET_USER_HANDLE = _L4_XCHG_REGS_SET_USER_HANDLE,
  VG_EXREGS_SET_REGS = (VG_EXREGS_SET_UTCB
			| VG_EXREGS_SET_EXCEPTION_MESSENGER
			| VG_EXREGS_SET_ASPACE
			| VG_EXREGS_SET_ACTIVITY
			| VG_EXREGS_SET_SP
			| VG_EXREGS_SET_IP
			| VG_EXREGS_SET_EFLAGS
			| VG_EXREGS_SET_USER_HANDLE),

  VG_EXREGS_GET_REGS = _L4_XCHG_REGS_DELIVER,

  VG_EXREGS_START = _L4_XCHG_REGS_SET_HALT,
  VG_EXREGS_STOP = _L4_XCHG_REGS_SET_HALT | _L4_XCHG_REGS_HALT,

  VG_EXREGS_ABORT_SEND = _L4_XCHG_REGS_CANCEL_SEND,
  VG_EXREGS_ABORT_RECEIVE = _L4_XCHG_REGS_CANCEL_RECV,
  VG_EXREGS_ABORT_IPC = VG_EXREGS_ABORT_SEND | _L4_XCHG_REGS_CANCEL_RECV,
};

struct vg_thread_exregs_in
{
  uintptr_t aspace_cap_properties_flags;
  struct vg_cap_properties aspace_cap_properties;

  uintptr_t sp;
  uintptr_t ip;
  uintptr_t eflags;
  uintptr_t user_handle;
};

struct vg_thread_exregs_out
{
  uintptr_t sp;
  uintptr_t ip;
  uintptr_t eflags;
  uintptr_t user_handle;
};

/* l4_exregs wrapper.  */
RPC (thread_exregs, 6, 1, 4,
     /* cap_t principal, cap_t thread, */
     uintptr_t, control, struct vg_thread_exregs_in, in,
     cap_t, aspace, cap_t, activity, cap_t, utcb, cap_t, exception_messenger,
     /* Out: */
     struct vg_thread_exregs_out, out,
     cap_t, aspace_out, cap_t, activity_out, cap_t, utcb_out,
     cap_t, exception_messenger_out)

static inline error_t
vg_thread_start (vg_addr_t thread)
{
  struct vg_thread_exregs_in in;
  struct vg_thread_exregs_out out;

  return vg_thread_exregs (VG_ADDR_VOID, thread,
			   VG_EXREGS_START | VG_EXREGS_ABORT_IPC,
			   in, VG_ADDR_VOID, VG_ADDR_VOID,
			   VG_ADDR_VOID, VG_ADDR_VOID,
			   &out, NULL, NULL, NULL, NULL);
}

static inline error_t
vg_thread_start_sp_ip (vg_addr_t thread, uintptr_t sp, uintptr_t ip)
{
  struct vg_thread_exregs_in in;
  struct vg_thread_exregs_out out;

  in.sp = sp;
  in.ip = ip;

  return vg_thread_exregs (VG_ADDR_VOID, thread,
			   VG_EXREGS_START | VG_EXREGS_ABORT_IPC
			   | VG_EXREGS_SET_SP_IP,
			   in, VG_ADDR_VOID, VG_ADDR_VOID,
			   VG_ADDR_VOID, VG_ADDR_VOID,
			   &out, NULL, NULL, NULL, NULL);
}

static inline error_t
vg_thread_stop (vg_addr_t thread)
{
  struct vg_thread_exregs_in in;
  struct vg_thread_exregs_out out;

  return vg_thread_exregs (VG_ADDR_VOID, thread,
			   VG_EXREGS_STOP | VG_EXREGS_ABORT_IPC,
			   in, VG_ADDR_VOID, VG_ADDR_VOID,
			   VG_ADDR_VOID, VG_ADDR_VOID,
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
  error_t err = vg_thread_id (VG_ADDR_VOID, VG_ADDR_VOID, &tid);
  if (err)
    return vg_niltid;
  return tid;
}

#define RPC_STUB_PREFIX vg_activation
#define RPC_ID_PREFIX VG_ACTIVATION
#include <viengoos/rpc.h>

/* Activation message ids.  */
enum
  {
    VG_ACTIVATION_fault = 10,
  };

/* Return a string corresponding to a message id.  */
static inline const char *
vg_activation_method_id_string (uintptr_t id)
{
  switch (id)
    {
    case VG_ACTIVATION_fault:
      return "fault";
    default:
      return "unknown";
    }
}

enum
  {
    VG_READ = 1 << 0,
#define VG_READ VG_READ
    VG_WRITE = 1 << 1,
#define VG_WRITE VG_WRITE
    VG_EXECUTE = 1 << 2,
#define VG_EXECUTE VG_EXECUTE
  };

struct vg_activation_fault_info
{
  union
  {
    struct
    {
      /* Type of access.  A bitwise or of VG_READ, VG_WRITE and
	 VG_EXECUTE.  */
      uintptr_t access: 3;
      /* Type of object that was attempting to be accessed.  */
      uintptr_t type : VG_CAP_TYPE_BITS;
      /* Whether the page was discarded.  */
      uintptr_t discarded : 1;
    };
    uintptr_t raw;
  };
};

#define VG_ACTIVATION_FAULT_INFO_FMT "%c%c%c %s%s"
#define VG_ACTIVATION_FAULT_INFO_PRINTF(info)		\
  ((info).access & VG_READ ? 'r' : '~'),	\
  ((info).access & VG_WRITE ? 'w' : '~'),	\
  ((info).access & VG_EXECUTE ? 'x' : '~'),	\
  vg_cap_type_string ((info).type),			\
  (info.discarded) ? " discarded" : ""

/* Raise a fault at address FAULT_ADDRESS.  If IP is not 0, then IP is
   the value of the IP of the faulting thread at the time of the fault
   and SP the value of the stack pointer at the time of the fault.  */
RPC (fault, 4, 0, 0,
     vg_addr_t, fault_address, uintptr_t, sp, uintptr_t, ip,
     struct vg_activation_fault_info, vg_activation_fault_info)

#undef RPC_STUB_PREFIX
#undef RPC_ID_PREFIX

#endif /* _VIENGOOS_THREAD_H */
#endif /* __need_vg_thread_id_t */
