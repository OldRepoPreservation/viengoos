#ifndef _L4_SPACE_H
# error "Never use <l4/bits/space.h> directly; include <l4/space.h> instead."
#endif

#ifndef _L4_EXTERN_INLINE
#define _L4_EXTERN_INLINE extern __inline
#endif


/* IO Fpages.  */

typedef _L4_RAW
(l4_word_t, _L4_STRUCT1
 ({
   _L4_BITFIELD4
     (l4_word_t,
      _L4_BITFIELD (rights, 4),
      _L4_BITFIELD (_two, 2),
      _L4_BITFIELD (log2_size, 6),
      _L4_BITFIELD_32_64 (base, 16, 48));
 })) l4_io_fpage_t;
  
_L4_EXTERN_INLINE l4_fpage_t
l4_io_fpage (l4_word_t base_address, int size)
{
  l4_fpage_t fpage;
  l4_io_fpage_t io_fpage;
  l4_word_t msb = __l4_msb (size);

  io_fpage.rights = 0;
  io_fpage._two = 2;
  io_fpage.log2_size = (1 << msb) == size ? msb : msb + 1;
  io_fpage.base = base_address;
  fpage.raw = io_fpage.raw;
  return fpage;
}


_L4_EXTERN_INLINE l4_fpage_t
l4_io_fpage_log2 (l4_word_t base_address, int log2_size)
{
  l4_fpage_t fpage;
  l4_io_fpage_t io_fpage;

  io_fpage.rights = 0;
  io_fpage._two = 2;
  io_fpage.log2_size = log2_size;
  io_fpage.base = base_address;
  fpage.raw = io_fpage.raw;
  return fpage;  
}


/* l4_space_control control argument.  */

#define L4_LARGE_SPACE		0
#define L4_SMALL_SPACE		(1 << 31)

/* LOC and SIZE are in MB.  */
_L4_EXTERN_INLINE l4_word_t
l4_small_space (l4_word_t loc, l4_word_t size)
{
  l4_word_t small_space = loc >> 1;	/* Divide by 2 (MB).  */
  l4_word_t two_pow_p = size >> 2;	/* Divide by 4 (MB).  */

  /* Make P the LSB of small_space.  */
  small_space = (small_space & ~(two_pow_p - 1)) | two_pow_p;
  return small_space & 0xff;
}
