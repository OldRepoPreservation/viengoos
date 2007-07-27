/* l4/abi/bits/kip.h - L4 KIP features for v2/ia32.
   Copyright (C) 2007 Free Software Foundation, Inc.
   Written by Neal H. Walfield <neal@gnu.org>.

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
# error "Never use <l4/abi/bits/kip.h> directly; include <l4/kip.h> instead."
#endif


/* The kernel interface page.  */
struct _L4_kip
{
  char magic[4];

  __L4_api_version_t api_version;
  _L4_uint8_t feature_string_offset_div_16;
  _L4_uint8_t __pad1[3];

  /* 0: No syscalls via KIP (must use software interrupts).
     1: syscalls via relative stubs
     2: syscalls via absolute stubs.  */
  _L4_uint8_t kip_sys_calls;
  _L4_uint8_t __pad2[3];
  
  _L4_word_t kdebug_init;
  _L4_word_t kdebug_exception;
  _L4_word_t sched_granularity;
  _L4_word_t kdebug_high;

  _L4_rootserver_t sigma0;
  _L4_rootserver_t sigma1;
  _L4_rootserver_t rootserver;

  _L4_word_t l4_config;

  _L4_RAW (_L4_word_t, _L4_STRUCT1 ({
    _L4_BITFIELD2
      (_L4_word_t,
       /* Number of memory descriptors.  */
       _L4_BITFIELD (nr, 16),

       /* Offset (in bytes) of memory descriptors in KIP.  */
       _L4_BITFIELD (mem_desc_ptr, 16));
  })) memory_info;

  _L4_word_t kdebug_config;
  _L4_word_t kdebug_permission;

  _L4_word_t total_ram;

  _L4_word_t __pad3[15];

  volatile _L4_clock_t clock;
  volatile _L4_clock_t switch_time;

  /* Frequency of CPU and Bus, in MHz.  */
  _L4_word_t internal_freq;
  _L4_word_t external_freq;

  volatile _L4_clock_t thread_time;

  /* Normal system call links.  */
  _L4_word_t ipc;
  _L4_word_t id_nearest;
  _L4_word_t fpage_unmap;
  _L4_word_t thread_switch;
  _L4_word_t thread_schedule;
  _L4_word_t lthread_ex_regs;
  _L4_word_t task_new;
  _L4_word_t privctrl;

  _L4_word_t boot_info;
  _L4_word_t vhw_offset;
  _L4_uint8_t vkey_irq;

  char __pad4[7];
};


/* The kernel description fields.  */
typedef char *_L4_kern_desc_t;


static inline _L4_api_flags_t
_L4_attribute_always_inline
_L4_api_flags (_L4_kip_t kip)
{
  __L4_api_flags_t flags;

  flags.endian = _L4_BYTE_ORDER == _L4_LITTLE_ENDIAN
    ? _L4_API_FLAGS_LITTLE_ENDIAN : _L4_API_FLAGS_BIG_ENDIAN;
  flags.wordsize = _L4_WORDSIZE == 32
    ? _L4_API_FLAGS_WORDSIZE_32 : _L4_API_FLAGS_WORDSIZE_64;

  return flags.raw;
}


static inline _L4_kern_desc_t
_L4_attribute_always_inline
_L4_kernel_desc (_L4_kip_t kip)
{
  /* We just return the version string on v2 and fabricate the
     rest.  */
  return (char *) ((_L4_word_t) kip
		   + (kip->feature_string_offset_div_16 << 4));
}


static inline _L4_kernel_id_t
_L4_attribute_always_inline
_L4_kernel_id (_L4_kip_t kip)
{
  __L4_kernel_id_t kid;

  /* XXX: There is no way to get this on a v2 kernel.  */
  kid.id = _L4_KERNEL_ID_FIASCO;
  kid.subid = _L4_KERNEL_SUBID_FIASCO;

  return kid.raw;
}


static inline void
_L4_attribute_always_inline
_L4_kernel_gen_date (_L4_kip_t kip,
		     _L4_word_t *year, _L4_word_t *month, _L4_word_t *day)
{
  /* XXX: There is no way to get this on a v2 kernel.  */
  *year = 0;
  *month = 0;
  *day = 0;
}


static inline __L4_kernel_version_t
_L4_attribute_always_inline
_L4_kernel_version (_L4_kip_t kip)
{
  /* XXX: There is no way to get this on a v2 kernel.  */
  __L4_kernel_version_t version;
  version.ver = _L4_API_VERSION_2;
  version.subver = 0;
  version.subsubver = 0;

  return version;
}


static inline char *
_L4_attribute_always_inline
_L4_kernel_supplier (_L4_kip_t kip)
{
  return (char[]) _L4_KERNEL_SUPPLIER_TUD;
}


static inline _L4_word_t
_L4_attribute_always_inline
_L4_num_processors (_L4_kip_t kip)
{
  return 1;
}


#define _L4_MIN_PAGE_SIZE_LOG2	12


static inline _L4_word_t
_L4_attribute_always_inline
_L4_page_size_mask (_L4_kip_t kip)
{
  /* We support 4k and 4m pages.  */
  return (1 << 22) | (1 << 12);
}


static inline _L4_word_t
_L4_attribute_always_inline
_L4_page_rights (_L4_kip_t kip)
{
  /* On v2, either r/w access or write access can be revoked.  */
  return 2;
}


static inline _L4_word_t
_L4_attribute_always_inline
_L4_thread_id_bits (_L4_kip_t kip)
{
  /* There are 11 bits for the task id and 7 bits for the thread
     id.  */
  return 11 + 7;
}


static inline _L4_word_t
_L4_attribute_always_inline
_L4_thread_system_base (_L4_kip_t kip)
{
  /* XXX: On v2, unlike on x2, tasks are first class.  */
  return 0;
}


static inline _L4_word_t
_L4_attribute_always_inline
_L4_thread_user_base (_L4_kip_t kip)
{
  return 0;
}


static inline _L4_word_t
_L4_attribute_always_inline
_L4_kip_area_size_log2 (_L4_kip_t kip)
{
  /* The KIP is exactly one page.  */
  return 12;
}


static inline char *
_L4_attribute_always_inline
_L4_kernel_version_string (_L4_kip_t kip)
{
  return (char *) ((_L4_word_t) kip
		   + (kip->feature_string_offset_div_16 << 4));
}
