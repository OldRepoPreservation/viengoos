#ifndef _L4_STUBS_INIT_H
# error "Never use <l4/bits/stubs-init.h> directly; include <l4/stubs-init.h> instead."
#endif

#define __L4_SETUP_SYSCALL(name)					\
extern void (*__l4_ ## name) (void);					\
  __l4_ ## name = (void (*) (void))					\
    (((l4_word_t) l4_get_kernel_interface ())				\
     + l4_get_kernel_interface ()->name)

/* Initialize the syscall stubs.  */
_L4_EXTERN_INLINE void
l4_init_stubs (void)
{
  __L4_SETUP_SYSCALL (exchange_registers);
  __L4_SETUP_SYSCALL (thread_control);
  __L4_SETUP_SYSCALL (system_clock);
  __L4_SETUP_SYSCALL (thread_switch);
  __L4_SETUP_SYSCALL (schedule);
  __L4_SETUP_SYSCALL (ipc);
  __L4_SETUP_SYSCALL (lipc);
  __L4_SETUP_SYSCALL (unmap);
  __L4_SETUP_SYSCALL (space_control);
  __L4_SETUP_SYSCALL (processor_control);
  __L4_SETUP_SYSCALL (memory_control);
};
