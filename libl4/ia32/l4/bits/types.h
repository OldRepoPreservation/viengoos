#ifndef _L4_TYPES_H
# error "Never use <l4/bits/types.h> directly; include <l4/types.h> instead."
#endif

/* ia32 has 32 bits per word.  */
#define L4_WORDSIZE	L4_WORDSIZE_32

/* ia32 is little-endian.  */
#define L4_BYTE_ORDER	L4_LITTLE_ENDIAN
