#ifndef _L4_MATH_H
# error "Never use <l4/bits/math.h> directly; include <l4/math.h> instead."
#endif


#ifndef _L4_EXTERN_INLINE
#define _L4_EXTERN_INLINE extern __inline
#endif


_L4_EXTERN_INLINE l4_word_t
__l4_msb_ (l4_word_t data) __attribute__((__const__));

/* Calculate the MSB set in DATA.  DATA is not 0.  */
_L4_EXTERN_INLINE l4_word_t
__l4_msb (l4_word_t data)
{
  l4_word_t msb;

  __asm__ ("bsr %[data], %[msb]"
	   : [msb] "=r" (msb)
	   : [data] "rm" (data));

  return msb + 1;
}


/* Calculate the LSB set in DATA.  DATA is not 0.  */
_L4_EXTERN_INLINE l4_word_t
__l4_lsb (l4_word_t data)
{
  l4_word_t lsb;

  __asm__ ("bsf %[data], %[lsb]"
	   : [lsb] "=r" (lsb)
	   : [data] "rm" (data));

  return lsb + 1;
}
