/* l4/gnu/kip.h - Public GNU interface to the L4 kernel interface page.
   Copyright (C) 2004, 2008 Free Software Foundation, Inc.
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
# error "Never use <l4/gnu/kip.h> directly; include <l4/kip.h> instead."
#endif

#include <l4/abi.h>

/* The kernel interface page.  */

typedef _L4_kip_t l4_kip_t;

/* This will be initialized by the linker to the task's kernel
   interface page.  */
extern l4_kip_t __l4_kip;

static inline l4_kip_t
_L4_attribute_always_inline
l4_kip (void)
{
  return __l4_kip;
}

typedef _L4_rootserver_t l4_rootserver_t;

#define L4_API_VERSION_2	_L4_API_VERSION_2
#define L4_API_VERSION_X0	_L4_API_VERSION_X0
#define L4_API_SUBVERSION_X0	_L4_API_SUBVERSION_X0
#define L4_API_VERSION_X1	_L4_API_VERSION_X1
#define L4_API_SUBVERSION_X1	_L4_API_SUBVERSION_X1
#define L4_API_VERSION_X2	_L4_API_VERSION_X2
/* FIXME: The official libl4 incorrectly(?) defines this.  */
#define L4_API_SUBVERSION_X2	(0x82)
#define L4_API_VERSION_L4SEC	_L4_API_VERSION_L4SEC
#define L4_API_VERSION_N1	_L4_API_VERSION_N1
#define L4_API_VERSION_2PP	_L4_API_VERSION_2PP
/* FIXME: The official libl4 lacks this one.  */
#define L4_API_VERSION_4	_L4_API_VERSION_4

typedef __L4_api_version_t l4_api_version_t;

/* Returns the API version.  */
static inline l4_api_version_t
_L4_attribute_always_inline
l4_api_version_from (l4_kip_t kip)
{
  __L4_api_version_t api_version;
  api_version.raw = _L4_api_version (kip);
  return api_version;
}

/* Paging information.  */

#define L4_MIN_PAGE_SIZE_LOG2	_L4_MIN_PAGE_SIZE_LOG2
#define L4_MIN_PAGE_SIZE	_L4_MIN_PAGE_SIZE


static inline l4_word_t
_L4_attribute_always_inline
l4_page_size_mask_from (l4_kip_t kip)
{
  return _L4_page_size_mask (kip);
}


static inline l4_word_t
l4_page_size_mask (void)
{
  return l4_page_size_mask_from (l4_kip ());
}


static inline l4_word_t
_L4_attribute_always_inline
l4_page_rights_from (l4_kip_t kip)
{
  return _L4_page_rights (kip);
}


static inline l4_word_t
l4_page_rights (void)
{
  return l4_page_rights_from (l4_kip ());
}


static inline l4_word_t
_L4_attribute_always_inline
l4_min_page_size_log2 (void)
{
  l4_word_t page_size_mask = l4_page_size_mask ();

  /* There'd better be one bit set.  */
  return l4_lsb (page_size_mask) - 1;
}


static inline l4_word_t
_L4_attribute_always_inline
l4_min_page_size (void)
{
  return L4_WORD_C(1) << l4_min_page_size_log2 ();
}


static inline l4_word_t
_L4_attribute_always_inline
l4_page_trunc (l4_word_t addr)
{
  return (addr & ~(l4_min_page_size () - 1));
}


static inline l4_word_t
_L4_attribute_always_inline
l4_page_round (l4_word_t addr)
{
  return ((addr + (l4_min_page_size () - 1)) 
	  & ~(l4_min_page_size () - 1));
}


static inline l4_word_t
_L4_attribute_always_inline
l4_atop (l4_word_t addr)
{
  return ((addr) >> l4_min_page_size_log2 ());
}

static inline l4_word_t
_L4_attribute_always_inline
l4_ptoa (l4_word_t p)
{
  return ((p) << l4_min_page_size_log2 ());
}


/* Thread ID information.  */

static inline l4_word_t
_L4_attribute_always_inline
l4_thread_id_bits_from (l4_kip_t kip)
{
  return _L4_thread_id_bits (kip);
}


static inline l4_word_t
l4_thread_id_bits (void)
{
  return l4_thread_id_bits_from (l4_kip ());
}


static inline l4_word_t
_L4_attribute_always_inline
l4_thread_system_base_from (l4_kip_t kip)
{
  return _L4_thread_system_base (kip);
}


static inline l4_word_t
l4_thread_system_base (void)
{
  return l4_thread_system_base_from (l4_kip ());
}


static inline l4_word_t
_L4_attribute_always_inline
l4_thread_user_base_from (l4_kip_t kip)
{
  return _L4_thread_user_base (kip);
}


static inline l4_word_t
l4_thread_user_base (void)
{
  return l4_thread_user_base_from (l4_kip ());
}


#ifdef _L4_X2
/* Scheduler information.  */

static inline l4_word_t
_L4_attribute_always_inline
l4_read_precision_from (l4_kip_t kip)
{
  return _L4_read_precision (kip);
}


static inline l4_word_t
l4_read_precision (void)
{
  return l4_read_precision_from (l4_kip ());
}


static inline l4_word_t
_L4_attribute_always_inline
l4_schedule_precision_from (l4_kip_t kip)
{
  return _L4_schedule_precision (kip);
}


static inline l4_word_t
l4_schedule_precision (void)
{
  return l4_schedule_precision_from (l4_kip ());
}
#endif /* _L4_X2.  */

#ifdef _L4_X2
/* UTCB area information.  */

static inline l4_word_t
_L4_attribute_always_inline
l4_utcb_area_size_log2_from (l4_kip_t kip)
{
  return _L4_utcb_area_size_log2 (kip);
}


static inline l4_word_t
_L4_attribute_always_inline
l4_utcb_area_size_log2 (void)
{
  return l4_utcb_area_size_log2_from (l4_kip ());
}

static inline l4_word_t
_L4_attribute_always_inline
l4_utcb_area_size (void)
{
  return L4_WORD_C(1) << l4_utcb_area_size_log2 ();
}


static inline l4_word_t
_L4_attribute_always_inline
l4_utcb_alignment_log2_from (l4_kip_t kip)
{
  return _L4_utcb_alignment_log2 (kip);
}


static inline l4_word_t
_L4_attribute_always_inline
l4_utcb_alignment_log2 (void)
{
  return l4_utcb_alignment_log2_from (l4_kip ());
}

static inline l4_word_t
_L4_attribute_always_inline
l4_utcb_alignment (void)
{
  return L4_WORD_C(1) << l4_utcb_alignment_log2 ();
}


static inline l4_word_t
_L4_attribute_always_inline
l4_utcb_size_from (l4_kip_t kip)
{
  return _L4_utcb_size (kip);
}


static inline _L4_word_t
_L4_attribute_always_inline
l4_utcb_size (void)
{
  return l4_utcb_size_from (l4_kip ());
}
#endif /* _L4_X2 */

/* Meta-information about the KIP.  */

static inline l4_word_t
_L4_attribute_always_inline
l4_kip_area_size_log2_from (l4_kip_t kip)
{
  return _L4_kip_area_size_log2 (kip);
}


static inline l4_word_t
_L4_attribute_always_inline
l4_kip_area_size_log2 (void)
{
  return l4_kip_area_size_log2_from (l4_kip ());
}

static inline l4_word_t
_L4_attribute_always_inline
l4_kip_area_size (void)
{
  return L4_WORD_C(1) << l4_kip_area_size_log2 ();
}


/* Boot information.  */

static inline l4_word_t
_L4_attribute_always_inline
l4_boot_info_from (l4_kip_t kip)
{
  return _L4_boot_info (kip);
}


static inline l4_word_t
_L4_attribute_always_inline
l4_boot_info (void)
{
  return l4_boot_info_from (l4_kip ());
}


/* Kernel information.  */

static inline void
_L4_attribute_always_inline
l4_kernel_gen_date_from (l4_kip_t kip, l4_word_t *year,
			 l4_word_t *month, l4_word_t *day)
{
  return _L4_kernel_gen_date (kip, year, month, day);
}


static inline void
_L4_attribute_always_inline
l4_kernel_gen_date (l4_word_t *year, l4_word_t *month, l4_word_t *day)
{
  l4_kernel_gen_date_from (l4_kip (), year, month, day);
}


static inline void
_L4_attribute_always_inline
l4_kernel_version_from (l4_kip_t kip, l4_word_t *ver, l4_word_t *subver,
			l4_word_t *subsubver)
{
  __L4_kernel_version_t version = _L4_kernel_version (kip);
  *ver = version.ver;
  *subver = version.subver;
  *subsubver = version.subsubver;
}


static inline void
_L4_attribute_always_inline
l4_kernel_version (l4_word_t *ver, l4_word_t *subver, l4_word_t *subsubver)
{
  return l4_kernel_version_from (l4_kip (), ver, subver, subsubver);
}


#define L4_KERNEL_SUPPLIER_GMD	_L4_KERNEL_SUPPLIER_GMD
#define L4_KERNEL_SUPPLIER_IBM	_L4_KERNEL_SUPPLIER_IBM
#define L4_KERNEL_SUPPLIER_UNSW	_L4_KERNEL_SUPPLIER_UNSW
#define L4_KERNEL_SUPPLIER_TUD	_L4_KERNEL_SUPPLIER_TUD
#define L4_KERNEL_SUPPLIER_UKA	_L4_KERNEL_SUPPLIER_UKA

static inline char *
_L4_attribute_always_inline
l4_kernel_supplier_from (l4_kip_t kip)
{
  return _L4_kernel_supplier (kip);
}


static inline char *
_L4_attribute_always_inline
l4_kernel_supplier (void)
{
  return l4_kernel_supplier_from (l4_kip ());
}


static inline char *
_L4_attribute_always_inline
l4_kernel_version_string_from (l4_kip_t kip)
{
  return _L4_kernel_version_string (kip);
}


static inline char *
_L4_attribute_always_inline
l4_kernel_version_string (void)
{
  return l4_kernel_version_string_from (l4_kip ());
}


static inline char *
_L4_attribute_always_inline
l4_feature_from (l4_kip_t kip, l4_word_t num)
{
  return _L4_feature (kip, num);
}


static inline char *
_L4_attribute_always_inline
l4_feature (l4_word_t num)
{
  return l4_feature_from (l4_kip (), num);
}


/* Memory descriptors.  */

typedef __L4_memory_desc_t l4_memory_desc_t;

#define L4_MEMDESC_UNDEFINED	_L4_MEMDESC_UNDEFINED
#define L4_MEMDESC_CONVENTIONAL	_L4_MEMDESC_CONVENTIONAL
#define L4_MEMDESC_RESERVED	_L4_MEMDESC_RESERVED
#define L4_MEMDESC_DEDICATED	_L4_MEMDESC_DEDICATED
#define L4_MEMDESC_SHARED	_L4_MEMDESC_SHARED
#define L4_MEMDESC_BOOTLOADER	_L4_MEMDESC_BOOTLOADER
#define L4_MEMDESC_ARCH		_L4_MEMDESC_ARCH

static inline l4_word_t
_L4_attribute_always_inline
l4_num_memory_desc_from (l4_kip_t kip)
{
  return _L4_num_memory_desc (kip);
}


static inline l4_word_t
_L4_attribute_always_inline
l4_num_memory_desc (void)
{
  return l4_num_memory_desc_from (l4_kip ());
}


static inline l4_memory_desc_t *
_L4_attribute_always_inline
l4_memory_desc_from (l4_kip_t kip, l4_word_t num)
{
  return _L4_memory_desc (kip, num);
}


static inline l4_memory_desc_t *
_L4_attribute_always_inline
l4_memory_desc (l4_word_t num)
{
  return l4_memory_desc_from (l4_kip (), num);
}


static inline bool
_L4_attribute_always_inline
l4_is_memory_desc_virtual (l4_memory_desc_t *mem)
{
  return _L4_is_memory_desc_virtual (mem);
}


static inline l4_word_t
_L4_attribute_always_inline
l4_memory_desc_type (l4_memory_desc_t *mem)
{
  return _L4_memory_desc_type (mem);
}

static inline const char *
l4_memory_desc_type_to_string (l4_word_t type)
{
  switch (type)
    {
    case L4_MEMDESC_UNDEFINED:
      return "undefined";
    case L4_MEMDESC_CONVENTIONAL:
      return "conventional";
    case L4_MEMDESC_RESERVED:
      return "reserved";
    case L4_MEMDESC_DEDICATED:
      return "dedicated";
    case L4_MEMDESC_SHARED:
      return "shared";
    case L4_MEMDESC_BOOTLOADER:
      return "bootloader";
    case L4_MEMDESC_ARCH:
      return "arch";
    default:
      return "unknown";
    }
}

static inline l4_word_t
_L4_attribute_always_inline
l4_memory_desc_low (l4_memory_desc_t *mem)
{
  return _L4_memory_desc_low (mem);
}


static inline l4_word_t
_L4_attribute_always_inline
l4_memory_desc_high (l4_memory_desc_t *mem)
{
  return _L4_memory_desc_high (mem);
}



/* Processor descriptors.  */

static inline l4_word_t
_L4_attribute_always_inline
l4_num_processors_from (l4_kip_t kip)
{
  return _L4_num_processors (kip);
}


static inline l4_word_t
_L4_attribute_always_inline
l4_num_processors (void)
{
  return l4_num_processors_from (l4_kip ());
}


#ifdef _L4_X2
typedef __L4_proc_desc_t l4_proc_desc_t;

static inline l4_proc_desc_t *
_L4_attribute_always_inline
l4_proc_desc_from (l4_kip_t kip, l4_word_t num)
{
  return _L4_proc_desc (kip, num);
}


static inline l4_proc_desc_t *
_L4_attribute_always_inline
l4_proc_desc (l4_word_t num)
{
  return l4_proc_desc_from (l4_kip (), num);
}


static inline l4_word_t
_L4_attribute_always_inline
l4_proc_external_freq (l4_proc_desc_t *proc)
{
  return _L4_proc_external_freq (proc);
}


static inline l4_word_t
_L4_attribute_always_inline
l4_proc_internal_freq (l4_proc_desc_t *proc)
{
  return _L4_proc_internal_freq (proc);
}
#endif /* _L4_X2 */
