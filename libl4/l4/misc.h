#ifndef _L4_MISC_H
#define _L4_MISC_H	1

#include <l4/types.h>
#include <l4/bits/misc.h>
#include <l4/vregs.h>
#include <l4/syscall.h>

#ifndef _L4_EXTERN_INLINE
#define _L4_EXTERN_INLINE extern __inline
#endif


/* l4_memory_control convenience interface.  */

#define L4_DEFAULT_MEMORY	0x0

_L4_EXTERN_INLINE void
l4_set_page_attribute (l4_fpage_t fpage, l4_word_t attribute)
{
  l4_set_rights (&fpage, 0);
  l4_load_mr (0, fpage.raw);
  l4_memory_control (0, &attribute); 
}


_L4_EXTERN_INLINE void
l4_set_pages_attributes (l4_word_t nr, l4_fpage_t *fpages,
			 l4_word_t *attributes)
{
  l4_load_mrs (0, nr, (l4_word_t *) fpages);
  l4_memory_control (nr - 1, attributes);
}

#endif	/* misc.h */
