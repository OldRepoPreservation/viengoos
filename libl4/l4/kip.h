/* l4/kip.h - Public interface to the L4 kernel interface page.
   Copyright (C) 2003, 2004, 2005, 2007 Free Software Foundation, Inc.
   Written by Marcus Brinkmann <marcus@gnu.org>.

   This file is part of the GNU L4 library.
   
   The GNU L4 library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public License
   as published by the Free Software Foundation; either version 3 of
   the License, or (at your option) any later version.
   
   Foobar is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.
   
   You should have received a copy of the GNU Lesser General Public
   License along with this program.  If not, see
   <http://www.gnu.org/licenses/>.  */

#ifndef _L4_KIP_H
#define _L4_KIP_H	1

#include <l4/features.h>
#include <l4/types.h>
#include <l4/math.h>

#include <l4/bits/kip.h>


/* The API version field.  */

#define _L4_API_VERSION_2	(0x02)
#define _L4_API_VERSION_X0	(0x83)
#define _L4_API_SUBVERSION_X0	(0x80)
#define _L4_API_VERSION_X1	(0x83)
#define _L4_API_SUBVERSION_X1	(0x81)
#define _L4_API_VERSION_X2	(0x84)
#define _L4_API_VERSION_L4SEC	(0x85)
#define _L4_API_VERSION_N1	(0x86)
#define _L4_API_VERSION_2PP	(0x87)
#define _L4_API_VERSION_4	(0x04)

typedef _L4_RAW (_L4_api_version_t, _L4_STRUCT1 ({
  _L4_BITFIELD4
    (_L4_word_t,
     _L4_BITFIELD (__pad1, 16),

     /* The subversion or revision.  */
     _L4_BITFIELD (subversion, 8),

     /* The interface version.  */
     _L4_BITFIELD (version, 8),
		  
     _L4_BITFIELD_64 (__pad2, 32));
})) __L4_api_version_t;


/* The API flag fields.  */

#define _L4_API_FLAGS_LITTLE_ENDIAN	(0x0)
#define _L4_API_FLAGS_BIG_ENDIAN	(0x1)
#define _L4_API_FLAGS_WORDSIZE_32	(0x0)
#define _L4_API_FLAGS_WORDSIZE_64	(0x1)

typedef _L4_RAW (_L4_api_flags_t, _L4_STRUCT1 ({
  _L4_BITFIELD3
    (_L4_word_t,
     /* The endianess.  */
     _L4_BITFIELD (endian, 2),

     /* The word size.  */
     _L4_BITFIELD (wordsize, 2),

     _L4_BITFIELD_32_64 (__pad, 28, 60));
})) __L4_api_flags_t;


/* The kernel ID field.  */

#define _L4_KERNEL_ID_L4_486			(0)
#define _L4_KERNEL_SUBID_L4_486			(1)
#define _L4_KERNEL_ID_L4_PENTIUM		(0)
#define _L4_KERNEL_SUBID_L4_PENTIUM		(2)
#define _L4_KERNEL_ID_L4_X86			(0)
#define _L4_KERNEL_SUBID_L4_X86			(3)
#define _L4_KERNEL_ID_L4_MIPS			(1)
#define _L4_KERNEL_SUBID_L4_MIPS		(1)
#define _L4_KERNEL_ID_L4_ALPHA			(2)
#define _L4_KERNEL_SUBID_L4_ALPHA		(1)
#define _L4_KERNEL_ID_FIASCO			(3)
#define _L4_KERNEL_SUBID_FIASCO			(1)
#define _L4_KERNEL_ID_L4KA_HAZELNUT		(4)
#define _L4_KERNEL_SUBID_L4KA_HAZELNUT		(1)
#define _L4_KERNEL_ID_L4KA_PISTACHIO		(4)
#define _L4_KERNEL_SUBID_L4KA_PISTACHIO		(2)
#define _L4_KERNEL_ID_L4KA_STRAWBERRY		(4)
#define _L4_KERNEL_SUBID_L4KA_STRAWBERRY	(3)


typedef _L4_RAW (_L4_kernel_id_t, _L4_STRUCT1 ({
  _L4_BITFIELD4
    (_L4_word_t,
     _L4_BITFIELD (__pad, 16),

     /* The kernel sub ID.  */
     _L4_BITFIELD (subid, 8),

     /* The kernel ID.  */
     _L4_BITFIELD (id, 8),

     _L4_BITFIELD_64 (__pad, 32));
})) __L4_kernel_id_t;


/* The kernel version.  */
typedef _L4_RAW (_L4_word_t, _L4_STRUCT1 ({
  _L4_BITFIELD4
    (_L4_word_t,
     _L4_BITFIELD (subsubver, 16),
     _L4_BITFIELD (subver, 8),
     _L4_BITFIELD (ver, 8),

     _L4_BITFIELD_64 (__pad, 32));
})) __L4_kernel_version_t;



/* The rootserver fields.  */
typedef struct
{
  _L4_word_t sp;
  _L4_word_t ip;
  _L4_word_t low;
  _L4_word_t high;
} _L4_rootserver_t;


/* The memory descriptor field.  */
typedef union _L4_memory_desc
{
  _L4_word_t raw[2];

  struct
  {
    _L4_BITFIELD5
    (_L4_word_t,
     /* The type of the memory descriptor.  */
     _L4_BITFIELD (type, 4),

     /* The subtype of the memory descriptor if type is
	L4_MEMDESC_BOOTLOADER or L4_MEMDESC_ARCH, otherwise
	undefined.  */
     _L4_BITFIELD (subtype, 4),

     _L4_BITFIELD (__pad1, 1),

     /* 1 if memory is virtual, 0 if it is physical.  */
     _L4_BITFIELD (virtual, 1),

     _L4_BITFIELD_32_64 (low, 22, 54));

    _L4_BITFIELD2
    (_L4_word_t,
     _L4_BITFIELD (__pad2, 10),

     _L4_BITFIELD_32_64 (high, 22, 54));
  };
} __L4_memory_desc_t;

typedef __L4_memory_desc_t *_L4_memory_desc_t;

#define _L4_MEMDESC_UNDEFINED		(0x0)
#define _L4_MEMDESC_CONVENTIONAL	(0x1)
#define _L4_MEMDESC_RESERVED		(0x2)
#define _L4_MEMDESC_DEDICATED		(0x3)
#define _L4_MEMDESC_SHARED		(0x4)
#define _L4_MEMDESC_BOOTLOADER		(0xe)
#define _L4_MEMDESC_ARCH		(0xf)


/* Values for the kernel supplier field.  */

#define _L4_KERNEL_SUPPLIER_GMD		{ 'G', 'M', 'D', ' ' }
#define _L4_KERNEL_SUPPLIER_IBM		{ 'I', 'B', 'M', ' ' }
#define _L4_KERNEL_SUPPLIER_UNSW	{ 'U', 'N', 'S', 'W' }
#define _L4_KERNEL_SUPPLIER_TUD		{ 'T', 'U', 'D', ' ' }
#define _L4_KERNEL_SUPPLIER_UKA		{ 'U', 'K', 'a', ' ' }


/* Include ABI specific types, etc.  */
#include <l4/abi/kip.h>

static inline _L4_api_version_t
_L4_attribute_always_inline
_L4_api_version (_L4_kip_t kip)
{
  return kip->api_version.raw;
}


#define _L4_MIN_PAGE_SIZE	(_L4_WORD_C(1) << _L4_MIN_PAGE_SIZE_LOG2)


static inline _L4_word_t
_L4_attribute_always_inline
_L4_boot_info (_L4_kip_t kip)
{
  return kip->boot_info;
}


static inline char *
_L4_attribute_always_inline
_L4_feature (_L4_kip_t kip, _L4_word_t num)
{
  char *feature = _L4_kernel_version_string (kip);

  do
    {
      while (*feature)
	feature++;
      feature++;
      if (!*feature)
	return (char *) 0;
    }
  while (num--);

  return feature;
}


static inline _L4_word_t
_L4_attribute_always_inline
_L4_num_memory_desc (_L4_kip_t kip)
{
  return kip->memory_info.nr;
}


static inline _L4_memory_desc_t
_L4_attribute_always_inline
_L4_memory_desc (_L4_kip_t kip, _L4_word_t num)
{
  _L4_memory_desc_t mem;

  if (num >= _L4_num_memory_desc (kip))
    return (_L4_memory_desc_t) 0;

  mem = (_L4_memory_desc_t) (((_L4_word_t) kip)
			     + kip->memory_info.mem_desc_ptr);
  return mem + num;
}


static inline _L4_word_t
_L4_attribute_always_inline
_L4_is_memory_desc_virtual (_L4_memory_desc_t mem)
{
  return mem->virtual;
}


static inline _L4_word_t
_L4_attribute_always_inline
_L4_memory_desc_type (_L4_memory_desc_t mem)
{
  return (mem->subtype << 4) + mem->type;
}


/* Return the address of the first byte of the memory region described
   by MEM.  */
static inline _L4_word_t
_L4_attribute_always_inline
_L4_memory_desc_low (_L4_memory_desc_t mem)
{
  /* The lower 10 bits are hard-wired to 0.  */
  return mem->low << 10;
}


/* Return the address of the last byte of the memory region described
   by MEM.  */
static inline _L4_word_t
_L4_attribute_always_inline
_L4_memory_desc_high (_L4_memory_desc_t mem)
{
  /* The lower 10 bits are hard-wired to 1.  */
  return (mem->high << 10) | ((1 << 10) - 1);
}


/* Now incorporate the public interfaces the user has selected.  */
#include <l4/syscall.h>
#ifdef _L4_INTERFACE_L4
#include <l4/compat/kip.h>
#endif
#ifdef _L4_INTERFACE_GNU
#include <l4/gnu/kip.h>
#endif

#endif	/* l4/kip.h */
