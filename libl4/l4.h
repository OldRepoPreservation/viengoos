#ifndef _L4_H
#define _L4_H	1

#include <l4/types.h>
#include <l4/syscall.h>
#include <l4/thread.h>
#include <l4/schedule.h>
#include <l4/space.h>
#include <l4/ipc.h>
#include <l4/misc.h>
#include <l4/kip.h>

#ifndef _L4_EXTERN_INLINE
#define _L4_EXTERN_INLINE extern __inline
#endif

/* Initialize the global data.  */
_L4_EXTERN_INLINE void
l4_init (void)
{
  l4_api_version_t version;
  l4_api_flags_t flags;
  l4_kernel_id_t id;

  __l4_kip = l4_kernel_interface (&version, &flags, &id);
};

#endif	/* l4.h */
