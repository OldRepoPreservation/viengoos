/* kip.h - Public interface for the L4 kernel interface page.
   Copyright (C) 2003 Free Software Foundation, Inc.
   Written by Marcus Brinkmann <marcus@gnu.org>.

   This file is part of the GNU L4 library.
 
   The GNU L4 library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public License
   as published by the Free Software Foundation; either version 2.1 of
   the License, or (at your option) any later version.
 
   The GNU L4 library is distributed in the hope that it will be
   useful, but WITHOUT ANY WARRANTY; without even the implied warranty
   of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU Lesser General Public License for more details.
 
   You should have received a copy of the GNU Lesser General Public
   License along with the GNU L4 library; if not, write to the Free
   Software Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
   02111-1307, USA.  */

#ifndef _L4_KIP_H
#define _L4_KIP_H	1

#include <l4/types.h>
#include <l4/bits/kip.h>


typedef _L4_RAW (l4_word_t, _L4_STRUCT1 ({
  _L4_BITFIELD4
    (l4_word_t,
     _L4_BITFIELD (__pad1, 16),

     /* The subversion or revision.  */
     _L4_BITFIELD (subversion, 8),

     /* The interface version.  */
     _L4_BITFIELD (version, 8),
		  
     _L4_BITFIELD_64 (__pad2, 32));
})) l4_api_version_t;


#define L4_API_FLAGS_LITTLE_ENDIAN	0x0
#define L4_API_FLAGS_BIG_ENDIAN		0x1
#define L4_API_FLAGS_WORDSIZE_32	0x0
#define L4_API_FLAGS_WORDSIZE_64	0x1

typedef _L4_RAW (l4_word_t, _L4_STRUCT1 ({
  _L4_BITFIELD3
    (l4_word_t,
     /* The endianess.  */
     _L4_BITFIELD (endian, 2),

     /* The word size.  */
     _L4_BITFIELD (wordsize, 2),

     _L4_BITFIELD_32_64 (__pad, 28, 60));
})) l4_api_flags_t;


typedef _L4_RAW (l4_word_t, _L4_STRUCT1 ({
  _L4_BITFIELD4
    (l4_word_t,
     _L4_BITFIELD (__pad, 16),

     /* The kernel sub ID.  */
     _L4_BITFIELD (subid, 8),

     /* The kernel ID.  */
     _L4_BITFIELD (id, 8),

     _L4_BITFIELD_64 (__pad, 32));
})) l4_kernel_id_t;


typedef _L4_RAW (l4_word_t, _L4_STRUCT1 ({
  _L4_BITFIELD5
    (l4_word_t,
     /* Execute access right can be independently set.  */
     _L4_BITFIELD (execute, 1),

     /* Write access right can be independently set.  */
     _L4_BITFIELD (write, 1),

     /* Read access right can be independently set.  */
     _L4_BITFIELD (read, 1),

     _L4_BITFIELD (__pad, 7),

     /* Page size of 2^(k + 10) is supported by hardware and kernel if
	bit k is set.  */
     _L4_BITFIELD_32_64 (page_size_mask, 22, 54));
})) l4_page_info_t;


typedef struct
{
  l4_word_t sp;
  l4_word_t ip;
  l4_word_t low;
  l4_word_t high;
} __l4_rootserver_t;

typedef struct
{
  char magic[4];
#if L4_WORDSIZE == L4_WORDSIZE_64
  char __pad1[4];
#endif

  l4_api_version_t api_version;
  l4_api_flags_t api_flags;

  l4_word_t kern_desc_ptr;

  struct
  {
    l4_word_t init;
    l4_word_t entry;
    l4_word_t low;
    l4_word_t high;
  } kdebug;

  __l4_rootserver_t sigma0;
  __l4_rootserver_t sigma1;
  __l4_rootserver_t rootserver;

  l4_word_t __pad2[1];

  _L4_RAW (l4_word_t, _L4_STRUCT1 ({
    _L4_BITFIELD2
      (l4_word_t,
       /* Number of memory descriptors.  */
       _L4_BITFIELD_32_64 (nr, 16, 32),

       /* Offset (in bytes) of memory descriptors in KIP.  */
       _L4_BITFIELD_32_64 (mem_desc_ptr, 16, 32));
  })) memory_info;

  l4_word_t kdebug_config[2];

  l4_word_t __pad3[18];

  _L4_RAW (l4_word_t, _L4_STRUCT1 ({
    _L4_BITFIELD4
      (l4_word_t,
       /* UTCB size multiplier.  Size of one UTCB block is m *
	  2^log2_align.  */
       _L4_BITFIELD (size_mul, 10),

       /* UTCB alignment requirement (must be aligned to 2^log2_align).  */
       _L4_BITFIELD (log2_align, 6),

       /* Minimal size for UTCB area is 2^log2_min_size.  */
       _L4_BITFIELD (log2_min_size, 6),

       _L4_BITFIELD_32_64 (__pad, 10, 42));
  })) utcb_info;

  _L4_RAW (l4_word_t, _L4_STRUCT1 ({
    _L4_BITFIELD2
      (l4_word_t,
       /* The size of the KIP area is 2^log2_size.  */
       _L4_BITFIELD (log2_size, 6),

       _L4_BITFIELD_32_64 (__pad, 26, 58));
  })) kip_area_info;

  l4_word_t __pad4[2];

  l4_word_t boot_info;

  /* Offset (in bytes) of processor descriptors in KIP.  */
  l4_word_t proc_desc_ptr;

  _L4_RAW (l4_word_t, _L4_STRUCT1 ({
    _L4_BITFIELD3
      (l4_word_t,
       /* Minimal time difference that can be read with the system
	  clock syscall.  */
       _L4_BITFIELD (read_precision, 16),

       /* Maximal jitter for a scheduled thread activation.  */
       _L4_BITFIELD (schedule_precision, 16),

       _L4_BITFIELD_64 (__pad, 32));
  })) clock_info;

  _L4_RAW (l4_word_t, _L4_STRUCT1 ({
    _L4_BITFIELD4
      (l4_word_t,
       /* Number of valid thread number bits.  */
       _L4_BITFIELD (log2_max_thread, 8),

       /* Lowest thread number used for system threads.  */
       _L4_BITFIELD (system_base, 12),

       /* Lowest thread number available for user threads.  */
       _L4_BITFIELD (user_base, 12),

       _L4_BITFIELD_64 (__pad, 32));
  })) thread_info;

  l4_page_info_t page_info;

  _L4_RAW (l4_word_t, _L4_STRUCT1 ({
    _L4_BITFIELD3
      (l4_word_t,
       /* The number of processors minus 1.  */
       _L4_BITFIELD (processors, 16),

       _L4_BITFIELD_32_64 (__pad, 12, 44),

       /* The size of one processor description is 2^log2_size.  */
       _L4_BITFIELD (log2_size, 4));
  })) processor_info;

  /* Privileged system call links.  */
  l4_word_t space_control;
  l4_word_t thread_control;
  l4_word_t processor_control;
  l4_word_t memory_control;

  /* Normal system call links.  */
  l4_word_t ipc;
  l4_word_t lipc;
  l4_word_t unmap;
  l4_word_t exchange_registers;
  l4_word_t system_clock;
  l4_word_t thread_switch;
  l4_word_t schedule;
} *l4_kip_t;


struct l4_memory_desc
{
  _L4_BITFIELD5
  (l4_word_t,
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
  (l4_word_t,
   _L4_BITFIELD (__pad2, 10),

   _L4_BITFIELD_32_64 (high, 22, 54));
};
typedef struct l4_memory_desc *l4_memory_desc_t;

#define L4_MEMDESC_MASK		0xf
#define L4_MEMDESC_UNDEFINED	0x0
#define L4_MEMDESC_CONVENTIONAL	0x1
#define L4_MEMDESC_RESERVED	0x2
#define L4_MEMDESC_DEDICATED	0x3
#define L4_MEMDESC_SHARED	0x4
#define L4_MEMDESC_BOOTLOADER	0xe
#define L4_MEMDESC_ARCH		0xf


typedef struct
{
  /* External frequency in kHz.  */
  l4_word_t external_freq;

  /* Internal frequency in kHz.  */
  l4_word_t internal_freq;

  l4_word_t __pad[2];
} *l4_proc_desc_t;


typedef struct
{
  /* Kernel ID.  */
  l4_kernel_id_t id;

  /* The kernel generation date.  */
  _L4_RAW (l4_word_t, _L4_STRUCT1 ({
    _L4_BITFIELD4
      (l4_word_t,
       _L4_BITFIELD (day, 5),
       _L4_BITFIELD (month, 4),
       /* The year from 2000 on.  */
       _L4_BITFIELD (year, 7),
       _L4_BITFIELD_32_64 (__pad, 16, 48));
  })) gen_date;

  /* The kernel version.  */
  _L4_RAW (l4_word_t, _L4_STRUCT1 ({
    _L4_BITFIELD4
      (l4_word_t,
       _L4_BITFIELD (subsubver, 16),
       _L4_BITFIELD (subver, 8),
       _L4_BITFIELD (ver, 8),

       _L4_BITFIELD_64 (__pad, 32));
  })) version;

  char supplier[4];
#if L4_WORDSIZE == L4_WORDSIZE_64
  char __pad[4];
#endif

  /* The kernel version string followed by architecture specific
     feature strings.  */
  char version_parts[0];
} *l4_kern_desc_t;


extern l4_kip_t __l4_kip;
#define l4_kip()  (__l4_kip + 0)  /* Not an lvalue.  */


#ifndef _L4_EXTERN_INLINE
#define _L4_EXTERN_INLINE extern __inline
#endif

_L4_EXTERN_INLINE 

_L4_EXTERN_INLINE l4_api_version_t
l4_api_version (void)
{
  return l4_kip ()->api_version;
}


_L4_EXTERN_INLINE l4_api_flags_t
l4_api_flags (void)
{
  return l4_kip ()->api_flags;
}


_L4_EXTERN_INLINE l4_kernel_id_t
l4_kernel_id (void)
{
  l4_kern_desc_t kern;

  kern = (l4_kern_desc_t) ((l4_word_t) l4_kip ()
			   + l4_kip ()->kern_desc_ptr);
  return kern->id;
}


_L4_EXTERN_INLINE void
l4_kernel_gen_date (l4_word_t *year, l4_word_t *month, l4_word_t *day)
{
  l4_kern_desc_t kern;

  kern = (l4_kern_desc_t) ((l4_word_t) l4_kip ()
			   + l4_kip ()->kern_desc_ptr);

  if (year)
    *year = kern->gen_date.year + 2000;
  if (month)
    *month = kern->gen_date.month;
  if (day)
    *day = kern->gen_date.day;
}


_L4_EXTERN_INLINE void
l4_kernel_version (l4_word_t *ver, l4_word_t *subver, l4_word_t *subsubver)
{
  l4_kern_desc_t kern;

  kern = (l4_kern_desc_t) ((l4_word_t) l4_kip ()
			   + l4_kip ()->kern_desc_ptr);

  if (ver)
    *ver = kern->version.ver;
  if (subver)
    *subver = kern->version.subver;
  if (subsubver)
    *subsubver = kern->version.subsubver;
}


_L4_EXTERN_INLINE char *
l4_kernel_supplier (void)
{
  l4_kern_desc_t kern;

  kern = (l4_kern_desc_t) ((l4_word_t) l4_kip ()
			   + l4_kip ()->kern_desc_ptr);

  return kern->supplier;
}


_L4_EXTERN_INLINE l4_word_t
l4_num_processors (void)
{
  return l4_kip ()->processor_info.processors + 1;
}


_L4_EXTERN_INLINE l4_proc_desc_t
l4_proc_desc (l4_word_t num)
{
  if (num >= l4_num_processors ())
    return (l4_proc_desc_t) 0;

  return (l4_proc_desc_t) ((l4_word_t) l4_kip ()
			   + l4_kip ()->proc_desc_ptr)
    + num * (1 << l4_kip ()->processor_info.log2_size);
}


_L4_EXTERN_INLINE l4_word_t
l4_proc_internal_freq (l4_proc_desc_t proc)
{
  return proc->internal_freq;
}


_L4_EXTERN_INLINE l4_word_t
l4_proc_external_freq (l4_proc_desc_t proc)
{
  return proc->external_freq;
}


#define L4_MIN_PAGE_SIZE_LOG2	10

_L4_EXTERN_INLINE l4_word_t
l4_page_size_mask (void)
{
  return l4_kip ()->page_info.page_size_mask
    << L4_MIN_PAGE_SIZE_LOG2;
}


_L4_EXTERN_INLINE l4_word_t l4_min_page_size_log2 (void)
     __attribute__((__const__));

_L4_EXTERN_INLINE l4_word_t
l4_min_page_size_log2 (void)
{
  page_size_mask = l4_kip ()->page_info.page_size_mask;
  unsigned int page_size_log2 = L4_MIN_PAGE_SIZE_LOG2;
  
  /* There'd better be one bit set.  */
  while (!(page_size_mask & 1))
    {
      page_size_log2++;
      page_size_mask >>= 1;
    }

  return page_size_log2;
}


_L4_EXTERN_INLINE l4_word_t l4_min_page_size_log2 (void)
     __attribute__((__const__));

_L4_EXTERN_INLINE l4_word_t
l4_min_page_size (void)
{
  return L4_WORD_C(1) << l4_min_page_size_log2 ();
}


_L4_EXTERN_INLINE l4_page_info_t
l4_page_rights (void)
{
  return l4_kip ()->page_info;
}


_L4_EXTERN_INLINE l4_word_t
l4_thread_id_bits (void)
{
  return l4_kip ()->thread_info.log2_max_thread;
}


_L4_EXTERN_INLINE l4_word_t
l4_thread_user_base (void)
{
  return l4_kip ()->thread_info.user_base;
}


_L4_EXTERN_INLINE l4_word_t
l4_thread_system_base (void)
{
  return l4_kip ()->thread_info.system_base;
}


_L4_EXTERN_INLINE l4_word_t
l4_read_precision (void)
{
  return l4_kip ()->clock_info.read_precision;
}


_L4_EXTERN_INLINE l4_word_t
l4_schedule_precision (void)
{
  return l4_kip ()->clock_info.schedule_precision;
}


_L4_EXTERN_INLINE l4_word_t
l4_utcb_area_size_log2 (void)
{
  return l4_kip ()->utcb_info.log2_min_size;
}


_L4_EXTERN_INLINE l4_word_t
l4_utcb_area_size (void)
{
  return 1 << l4_kip ()->utcb_info.log2_min_size;
}


_L4_EXTERN_INLINE l4_word_t
l4_utcb_alignment_log2 (void)
{
  return l4_kip ()->utcb_info.log2_align;
}


_L4_EXTERN_INLINE l4_word_t
l4_utcb_size (void)
{
  return l4_kip ()->utcb_info.size_mul
    * (1 << l4_utcb_alignment_log2 ());
}


_L4_EXTERN_INLINE l4_word_t
l4_kip_area_size_log2 (void)
{
  return l4_kip ()->kip_area_info.log2_size;
}


_L4_EXTERN_INLINE l4_word_t
l4_kip_area_size (void)
{
  return 1 << l4_kip ()->kip_area_info.log2_size;
}


_L4_EXTERN_INLINE l4_word_t
l4_boot_info (void)
{
  return l4_kip ()->boot_info;
}


_L4_EXTERN_INLINE char *
l4_kernel_version_string (void)
{
  l4_kern_desc_t kern;

  kern = (l4_kern_desc_t) ((l4_word_t) l4_kip ()
			   + l4_kip ()->kern_desc_ptr);

  return kern->version_parts;
}


_L4_EXTERN_INLINE char *
l4_feature (l4_word_t num)
{
  char *feature = l4_kernel_version_string ();

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


_L4_EXTERN_INLINE l4_word_t
l4_num_memory_desc (void)
{
  return l4_kip ()->memory_info.nr;
}


_L4_EXTERN_INLINE l4_memory_desc_t
l4_memory_desc (l4_word_t num)
{
  l4_memory_desc_t mem;

  if (num >= l4_num_memory_desc ())
    return (l4_memory_desc_t) 0;

  mem = (l4_memory_desc_t)
    ((l4_word_t) l4_kip ()
     + l4_kip ()->memory_info.mem_desc_ptr);
  return mem + num;
}


_L4_EXTERN_INLINE l4_word_t
l4_is_memory_desc_virtual (l4_memory_desc_t mem)
{
  return mem->virtual;
}


_L4_EXTERN_INLINE l4_word_t
l4_memory_desc_type (l4_memory_desc_t mem)
{
  return (mem->subtype << 4) + mem->type;
}


_L4_EXTERN_INLINE l4_word_t
l4_memory_desc_low (l4_memory_desc_t mem)
{
  return mem->low << 10;
}


_L4_EXTERN_INLINE l4_word_t
l4_memory_desc_high (l4_memory_desc_t mem)
{
  return mem->high << 10;
}

#endif	/* l4/kip.h */
