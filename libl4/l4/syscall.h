#ifndef _L4_SYSCALL_H
#define _L4_SYSCALL_H	1

#include <l4/types.h>
#include <l4/vregs.h>
#include <l4/kip.h>

/* <l4/bits/syscall.h> defines extern inlines for all system calls.  */
#include <l4/bits/syscall.h>

/* l4_exchange_registers control argument.  */

/* Input.  */
#define L4_XCHG_REGS_HALT		0x0001
#define L4_XCHG_REGS_CANCEL_RECV	0x0002
#define L4_XCHG_REGS_CANCEL_SEND	0x0004
#define L4_XCHG_REGS_CANCEL_IPC		(L4_XCHG_REGS_CANCEL_RECV	\
					 | L4_XCHG_REGS_CANCEL_SEND)
#define L4_XCHG_REGS_SET_SP		0x0008
#define L4_XCHG_REGS_SET_IP		0x0010
#define L4_XCHG_REGS_SET_FLAGS		0x0020
#define L4_XCHG_REGS_SET_USER_HANDLE	0x0040
#define L4_XCHG_REGS_SET_PAGER		0x0080
#define L4_XCHG_REGS_SET_HALT		0x0100

/* Output.  */
#define L4_XCHG_REGS_HALTED		0x01
#define L4_XCHG_REGS_RECEIVING		0x02
#define L4_XCHG_REGS_SENDING		0x04
#define L4_XCHG_REGS_IPCING		(L4_XCHG_REGS_RECEIVING		\
					 | L4_XCHG_REGS_SENDING)

/* l4_schedule return codes.  */
#define L4_SCHEDULE_ERROR		0
#define L4_SCHEDULE_DEAD		1
#define L4_SCHEDULE_INACTIVE		2
#define L4_SCHEDULE_RUNNING		3
#define L4_SCHEDULE_PENDING_SEND	4
#define L4_SCHEDULE_SENDING		5
#define L4_SCHEDULE_WAITING		6
#define L4_SCHEDULE_RECEIVING		7


/* l4_unmap flags.  */
#define L4_UNMAP_FLUSH		0x40
#define L4_UNMAP_COUNT_MASK	0x3f

#ifndef _L4_NOT_COMPAT
#define L4_ExchangeRegisters	l4_exchange_registers
#define L4_ThreadControl	l4_thread_control
#define L4_SystemClock() \
 ({ return ((L4_Clock_t) { .clock = l4_system_clock () })
#define L4_ThreadSwitch		l4_thread_switch
#define L4_Schedule		l4_schedule
#define L4_Ipc			l4_ipc
#define L4_Lipc			l4_lipc
#define L4_Unmap		l4_unmap
#define L4_SpaceControl		l4_space_control
#define L4_ProcessorControl	l4_processor_control
#define L4_MemoryControl	l4_memory_control
#endif	/* !_L4_NOT_COMPAT */

#endif	/* l4/syscall.h */
