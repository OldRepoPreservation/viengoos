#ifndef _L4_MATH_H
#define _L4_MATH_H	1

#include <l4/types.h>

/* <l4/bits/math.h> defines __l4_msb_().  */
#include <l4/bits/math.h>


#ifndef _L4_EXTERN_INLINE
#define _L4_EXTERN_INLINE extern __inline
#endif


_L4_EXTERN_INLINE l4_word_t
__l4_msb (l4_word_t data) __attribute__((__const__));

_L4_EXTERN_INLINE l4_word_t
__l4_msb (l4_word_t data)
{
  if (__builtin_constant_p (data))
    {
#define __L4_MSB_TRY(b) else if (data < (1 << (b + 1))) return (b)
#define __L4_MSB_IS(b) else return (b)

      if (!data)
	/* Undefined.  */
	return 0;
      __L4_MSB_TRY(0); __L4_MSB_TRY(1); __L4_MSB_TRY(2); __L4_MSB_TRY(3);
      __L4_MSB_TRY(4); __L4_MSB_TRY(5); __L4_MSB_TRY(6); __L4_MSB_TRY(7);
      __L4_MSB_TRY(8); __L4_MSB_TRY(9); __L4_MSB_TRY(10); __L4_MSB_TRY(11);
      __L4_MSB_TRY(12); __L4_MSB_TRY(13); __L4_MSB_TRY(14); __L4_MSB_TRY(15);
      __L4_MSB_TRY(16); __L4_MSB_TRY(17); __L4_MSB_TRY(18); __L4_MSB_TRY(19);
      __L4_MSB_TRY(20); __L4_MSB_TRY(21); __L4_MSB_TRY(22); __L4_MSB_TRY(23);
      __L4_MSB_TRY(24); __L4_MSB_TRY(25); __L4_MSB_TRY(26); __L4_MSB_TRY(27);
      __L4_MSB_TRY(28); __L4_MSB_TRY(29); __L4_MSB_TRY(30);
#if L4_WORDSIZE == L4_WORDSIZE_32
      __L4_MSB_IS(31);
#else
      __L4_MSB_TRY(31); __L4_MSB_TRY(32); __L4_MSB_TRY(33); __L4_MSB_TRY(34);
      __L4_MSB_TRY(35); __L4_MSB_TRY(36); __L4_MSB_TRY(37); __L4_MSB_TRY(38);
      __L4_MSB_TRY(39); __L4_MSB_TRY(40); __L4_MSB_TRY(41); __L4_MSB_TRY(42);
      __L4_MSB_TRY(43); __L4_MSB_TRY(44); __L4_MSB_TRY(45); __L4_MSB_TRY(46);
      __L4_MSB_TRY(47); __L4_MSB_TRY(48); __L4_MSB_TRY(49); __L4_MSB_TRY(50);
      __L4_MSB_TRY(51); __L4_MSB_TRY(52); __L4_MSB_TRY(53); __L4_MSB_TRY(54);
      __L4_MSB_TRY(55); __L4_MSB_TRY(56); __L4_MSB_TRY(57); __L4_MSB_TRY(58);
      __L4_MSB_TRY(59); __L4_MSB_TRY(60); __L4_MSB_TRY(61); __L4_MSB_TRY(62);
      __L4_MSB_IS(63);
#endif
    }
  return __l4_msb_ (data);
}

#endif	/* l4/math.h */
