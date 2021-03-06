/* l4/abi/kip.h - Public interface to the L4 kernel interface page, x2 
   Copyright (C) 2003, 2004, 2005, 2007 Free Software Foundation, Inc.
   Written by Marcus Brinkmann <marcus@gnu.org>.

   This file is part of GNU L4 library.
   
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
# error "Never use <l4/abi/kip.h> directly; include <l4/kip.h> instead."
#endif

/* The page info field.  */
typedef _L4_word_t _L4_page_info_t;

typedef _L4_RAW (_L4_page_info_t, _L4_STRUCT2 ({
  _L4_BITFIELD5
    (_L4_word_t,
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
},
{
  _L4_BITFIELD2
    (_L4_word_t,
     /* All access rights.  */
     _L4_BITFIELD (rwx, 3),

     _L4_BITFIELD_32_64 (__pad2, 29, 61));
})) __L4_page_info_t;


/* The kernel interface page.  */
struct _L4_kip
{
  char magic[4];
#if _L4_WORDSIZE == 64
  char __pad1[4];
#endif

  __L4_api_version_t api_version;
  __L4_api_flags_t api_flags;

  _L4_word_t kern_desc_ptr;

  struct
  {
    _L4_word_t init;
    _L4_word_t entry;
    _L4_word_t low;
    _L4_word_t high;
  } kdebug;

  _L4_rootserver_t sigma0;
  _L4_rootserver_t sigma1;
  _L4_rootserver_t rootserver;

  _L4_word_t __pad2[1];

  _L4_RAW (_L4_word_t, _L4_STRUCT1 ({
    _L4_BITFIELD2
      (_L4_word_t,
       /* Number of memory descriptors.  */
       _L4_BITFIELD_32_64 (nr, 16, 32),

       /* Offset (in bytes) of memory descriptors in KIP.  */
       _L4_BITFIELD_32_64 (mem_desc_ptr, 16, 32));
  })) memory_info;

  _L4_word_t kdebug_config[2];

  _L4_word_t __pad3[18];

  _L4_RAW (_L4_word_t, _L4_STRUCT1 ({
    _L4_BITFIELD4
      (_L4_word_t,
       /* UTCB size multiplier.  Size of one UTCB block is m *
	  2^log2_align.  */
       _L4_BITFIELD (size_mul, 10),

       /* UTCB alignment requirement (must be aligned to 2^log2_align).  */
       _L4_BITFIELD (log2_align, 6),

       /* Minimal size for UTCB area is 2^log2_min_size.  */
       _L4_BITFIELD (log2_min_size, 6),

       _L4_BITFIELD_32_64 (__pad, 10, 42));
  })) utcb_info;

  _L4_RAW (_L4_word_t, _L4_STRUCT1 ({
    _L4_BITFIELD2
      (_L4_word_t,
       /* The size of the KIP area is 2^log2_size.  */
       _L4_BITFIELD (log2_size, 6),

       _L4_BITFIELD_32_64 (__pad, 26, 58));
  })) kip_area_info;

  _L4_word_t __pad4[2];

  _L4_word_t boot_info;

  /* Offset (in bytes) of processor descriptors in KIP.  */
  _L4_word_t proc_desc_ptr;

  _L4_RAW (_L4_word_t, _L4_STRUCT1 ({
    _L4_BITFIELD3
      (_L4_word_t,
       /* Minimal time difference that can be read with the system
	  clock syscall.  */
       _L4_BITFIELD (read_precision, 16),

       /* Maximal jitter for a scheduled thread activation.  */
       _L4_BITFIELD (schedule_precision, 16),

       _L4_BITFIELD_64 (__pad, 32));
  })) clock_info;

  _L4_RAW (_L4_word_t, _L4_STRUCT1 ({
    _L4_BITFIELD4
      (_L4_word_t,
       /* Number of valid thread number bits.  */
       _L4_BITFIELD (log2_max_thread, 8),

       /* Lowest thread number used for system threads.  */
       _L4_BITFIELD (system_base, 12),

       /* Lowest thread number available for user threads.  */
       _L4_BITFIELD (user_base, 12),

       _L4_BITFIELD_64 (__pad, 32));
  })) thread_info;

  __L4_page_info_t page_info;

  _L4_RAW (_L4_word_t, _L4_STRUCT1 ({
    _L4_BITFIELD3
      (_L4_word_t,
       /* The number of processors minus 1.  */
       _L4_BITFIELD (processors, 16),

       _L4_BITFIELD_32_64 (__pad, 12, 44),

       /* The size of one processor description is 2^log2_size.  */
       _L4_BITFIELD (log2_size, 4));
  })) processor_info;

  /* Privileged system call links.  */
  _L4_word_t space_control;
  _L4_word_t thread_control;
  _L4_word_t processor_control;
  _L4_word_t memory_control;

  /* Normal system call links.  */
  _L4_word_t ipc;
  _L4_word_t lipc;
  _L4_word_t unmap;
  _L4_word_t exchange_registers;
  _L4_word_t system_clock;
  _L4_word_t thread_switch;
  _L4_word_t schedule;

  _L4_word_t _pad5[5];
  
  _L4_word_t arch0;
  _L4_word_t arch1;
  _L4_word_t arch2;
  _L4_word_t arch3;
};


/* The processor descriptor field.  */
typedef union _L4_proc_desc
{
  _L4_word_t raw[2];
  struct
  {
    /* External frequency in kHz.  */
    _L4_word_t external_freq;

    /* Internal frequency in kHz.  */
    _L4_word_t internal_freq;

    _L4_word_t __pad[2];
  };
} __L4_proc_desc_t;

typedef __L4_proc_desc_t *_L4_proc_desc_t;


/* The kernel description fields.  */
typedef struct
{
  /* Kernel ID.  */
  __L4_kernel_id_t id;

  /* The kernel generation date.  */
  _L4_RAW (_L4_word_t, _L4_STRUCT1 ({
    _L4_BITFIELD4
      (_L4_word_t,
       _L4_BITFIELD (day, 5),
       _L4_BITFIELD (month, 4),
       /* The year from 2000 on.  */
       _L4_BITFIELD (year, 7),
       _L4_BITFIELD_32_64 (__pad, 16, 48));
  })) gen_date;

  __L4_kernel_version_t version;

  _L4_word_t supplier;

  /* The kernel version string followed by architecture specific
     feature strings.  */
  char version_parts[0];
} __L4_kern_desc_t;

typedef __L4_kern_desc_t *_L4_kern_desc_t;


static inline _L4_api_flags_t
_L4_attribute_always_inline
_L4_api_flags (_L4_kip_t kip)
{
  return kip->api_flags.raw;
}


static inline _L4_kern_desc_t
_L4_attribute_always_inline
_L4_kernel_desc (_L4_kip_t kip)
{
  return (_L4_kern_desc_t) (((_L4_word_t) kip) + kip->kern_desc_ptr);
}


static inline _L4_kernel_id_t
_L4_attribute_always_inline
_L4_kernel_id (_L4_kip_t kip)
{
  return  _L4_kernel_desc (kip)->id.raw;
}


static inline void
_L4_attribute_always_inline
_L4_kernel_gen_date (_L4_kip_t kip,
                    _L4_word_t *year, _L4_word_t *month, _L4_word_t *day)
{
  _L4_kern_desc_t kern = _L4_kernel_desc (kip);

  *year = kern->gen_date.year + 2000;
  *month = kern->gen_date.month;
  *day = kern->gen_date.day;
}


static inline __L4_kernel_version_t
_L4_attribute_always_inline
_L4_kernel_version (_L4_kip_t kip)
{
  return _L4_kernel_desc (kip)->version;
}


static inline char *
_L4_attribute_always_inline
_L4_kernel_supplier (_L4_kip_t kip)
{
  _L4_kern_desc_t kern = _L4_kernel_desc (kip);

  return (char *) &kern->supplier;
}


static inline _L4_word_t
_L4_attribute_always_inline
_L4_num_processors (_L4_kip_t kip)
{
  return kip->processor_info.processors + 1;
}


static inline _L4_proc_desc_t
_L4_attribute_always_inline
_L4_proc_desc (_L4_kip_t kip, _L4_word_t num)
{
  return (_L4_proc_desc_t) (((_L4_word_t) kip) + kip->proc_desc_ptr
			    + num * (1 << kip->processor_info.log2_size));
}


static inline _L4_word_t
_L4_attribute_always_inline
_L4_proc_internal_freq (_L4_proc_desc_t proc)
{
  return proc->internal_freq;
}


static inline _L4_word_t
_L4_attribute_always_inline
_L4_proc_external_freq (_L4_proc_desc_t proc)
{
  return proc->external_freq;
}


#define _L4_MIN_PAGE_SIZE_LOG2	10


static inline _L4_word_t
_L4_attribute_always_inline
_L4_page_size_mask (_L4_kip_t kip)
{
  return kip->page_info.page_size_mask << _L4_MIN_PAGE_SIZE_LOG2;
}


static inline _L4_word_t
_L4_attribute_always_inline
_L4_page_rights (_L4_kip_t kip)
{
  return kip->page_info.rwx;
}


static inline _L4_word_t
_L4_attribute_always_inline
_L4_thread_id_bits (_L4_kip_t kip)
{
  return kip->thread_info.log2_max_thread;
}


static inline _L4_word_t
_L4_attribute_always_inline
_L4_thread_system_base (_L4_kip_t kip)
{
  return kip->thread_info.system_base;
}


static inline _L4_word_t
_L4_attribute_always_inline
_L4_thread_user_base (_L4_kip_t kip)
{
  return kip->thread_info.user_base;
}


static inline _L4_word_t
_L4_attribute_always_inline
_L4_read_precision (_L4_kip_t kip)
{
  return kip->clock_info.read_precision;
}


static inline _L4_word_t
_L4_attribute_always_inline
_L4_schedule_precision (_L4_kip_t kip)
{
  return kip->clock_info.schedule_precision;
}


static inline _L4_word_t
_L4_attribute_always_inline
_L4_utcb_area_size_log2 (_L4_kip_t kip)
{
  return kip->utcb_info.log2_min_size;
}


static inline _L4_word_t
_L4_attribute_always_inline
_L4_utcb_alignment_log2 (_L4_kip_t kip)
{
  return kip->utcb_info.log2_align;
}


static inline _L4_word_t
_L4_attribute_always_inline
_L4_utcb_size (_L4_kip_t kip)
{
  return kip->utcb_info.size_mul
    * (_L4_WORD_C(1) << _L4_utcb_alignment_log2 (kip));
}


static inline _L4_word_t
_L4_attribute_always_inline
_L4_kip_area_size_log2 (_L4_kip_t kip)
{
  return kip->kip_area_info.log2_size;
}


static inline char *
_L4_attribute_always_inline
_L4_kernel_version_string (_L4_kip_t kip)
{
  return _L4_kernel_desc (kip)->version_parts;
}
