/* Define the real-function versions of all inline functions defined
   in l4/stubs-init.h.  */

/* Include all header files used by l4/stubs-init.h (or
   l4/bits/stubs-init.h) which possibly define extern inline functions
   as well before setting _L4_EXTERN_INLINE.  */
#include <l4/kip.h>

#ifdef _L4_EXTERN_INLINE
#undef _L4_EXTERN_INLINE
#endif
#define _L4_EXTERN_INLINE

#include <l4/stubs-init.h>
