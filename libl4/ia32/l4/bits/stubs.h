#ifndef _L4_STUBS_H
# error "Never use <l4/bits/stubs.h> directly; include <l4/stubs.h> instead."
#endif

typedef void (*__l4_syscall_stub_t) (void);

__l4_syscall_stub_t __l4_exchange_registers;
__l4_syscall_stub_t __l4_thread_control;
__l4_syscall_stub_t __l4_system_clock;
__l4_syscall_stub_t __l4_thread_switch;
__l4_syscall_stub_t __l4_schedule;
__l4_syscall_stub_t __l4_ipc;
__l4_syscall_stub_t __l4_lipc;
__l4_syscall_stub_t __l4_unmap;
__l4_syscall_stub_t __l4_space_control;
__l4_syscall_stub_t __l4_processor_control;
__l4_syscall_stub_t __l4_memory_control;
