#ifndef __SYS_LOCK_H__
#define __SYS_LOCK_H__

#define __need_ss_rmutex_t
#include <hurd/rmutex.h>

#define __need_ss_mutex_t
#include <hurd/mutex.h>

#include <hurd/stddef.h>

typedef ss_mutex_t _LOCK_T;

extern int __newlib_trace_locks;

#define __LOCK_INIT(__class, __lock) __class ss_mutex_t __lock = 0
#define __lock_init(__lock) __lock = 0

extern void __lock_release_ (_LOCK_T *lockp);
#define __lock_release(l)						\
  ({									\
    debug (__newlib_trace_locks ? 0 : 5, "lock_release %p", &(l));	\
    __lock_release_ (&(l));						\
  })

extern void __lock_acquire_ (_LOCK_T *lockp);
#define __lock_acquire(l)						\
  ({									\
    debug (__newlib_trace_locks ? 0 : 5, "lock_acquire %p", &(l));	\
    __lock_acquire_ (&(l));						\
  })


/* Returns 0 if the lock was acquired.  */
extern int __lock_try_acquire_ (_LOCK_T *lockp);
#define __lock_try_acquire(l)						\
  ({									\
    debug (__newlib_trace_locks ? 0 : 5, "lock_try_acquire %p", &(l));	\
    __lock_try_acquire_ (&(l));						\
  })

#define __lock_close(__lock) do { } while (0)

typedef ss_rmutex_t _LOCK_RECURSIVE_T;

#define __LOCK_INIT_RECURSIVE(__class, __lock) \
  __class ss_rmutex_t __lock = { 0, 0, 0 }

#define __lock_init_recursive(__lock)			\
  __lock = (_LOCK_RECURSIVE_T) { 0, 0, 0 }

extern void __lock_acquire_recursive_ (_LOCK_RECURSIVE_T *lockp);
#define __lock_acquire_recursive(l)					\
  ({									\
    debug (__newlib_trace_locks ? 0 : 5,				\
	   "lock_acquire_recursive %p", &(l));				\
    __lock_acquire_recursive_ (&(l));					\
  })

/* Returns 0 if the lock was acquired.  */
extern int __lock_try_acquire_recursive_ (_LOCK_RECURSIVE_T *lockp);
#define __lock_try_acquire_recursive(l)					\
  ({									\
    debug (__newlib_trace_locks ? 0 : 5,				\
	   "lock_try_acquire_recursive %p", &(l));			\
    __lock_try_acquire_recursive_ (&(l));				\
  })

extern void __lock_release_recursive_ (_LOCK_RECURSIVE_T *lockp);
#define __lock_release_recursive(l)					\
  ({									\
    debug (__newlib_trace_locks ? 0 : 5, "lock_release_recursive %p", &(l)); \
    __lock_release_recursive_ (&(l));				\
  })

#define __lock_close_recursive(__lock) do { } while (0)

#endif /* __SYS_LOCK_H__ */
