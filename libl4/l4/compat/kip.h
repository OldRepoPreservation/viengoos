/* l4/compat/kip.h - Public interface for L4 types.
   Copyright (C) 2004 Free Software Foundation, Inc.
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
# error "Never use <l4/compat/kip.h> directly; include <l4/kip.h> instead."
#endif


/* 1.2 Kernel Interface [Slow Systemcall]  */

/* Generic Programming Interface.  */

static inline void *
_L4_attribute_always_inline
L4_KernelInterface (L4_Word_t *api_version, L4_Word_t *api_flags,
		    L4_Word_t *kernel_id)
{
  return _L4_kernel_interface (api_version, api_flags, kernel_id);
}


/* Convenience Programming Interface.  */

typedef struct
{
  L4_Word_t raw[2];
} L4_MemoryDesc_t;


typedef struct
{
  L4_Word_t raw[2];
} L4_ProcDesc_t;


/* Returns the address of the kernel interface page.  */
static inline void *
_L4_attribute_always_inline
#if defined(__cplusplus)
L4_KernelInterface (void)
#else
L4_GetKernelInterface (void)
#endif
{
  _L4_api_version_t api_version;
  _L4_api_flags_t api_flags;
  _L4_kernel_id_t kernel_id;

  return _L4_kernel_interface (&api_version, &api_flags, &kernel_id);
}


#define L4_APIVERSION_2		_L4_API_VERSION_2
#define L4_APIVERSION_X0	_L4_API_VERSION_X0
#define L4_APISUBVERSION_X0	_L4_API_SUBVERSION_X0
#define L4_APIVERSION_X1	_L4_API_VERSION_X1
#define L4_APISUBVERSION_X1	_L4_API_SUBVERSION_X1
#define L4_APIVERSION_X2	_L4_API_VERSION_X2
/* FIXME: The official libl4 incorrectly(?) defines this.  */
#define L4_APISUBVERSION_X2	(0x82)
/* FIXME: The official libl4 lacks this one.  */
#define L4_APIVERSION_4		_L4_API_VERSION_4

/* Returns the API version.  */
static inline L4_Word_t
_L4_attribute_always_inline
L4_ApiVersion (void)
{
  L4_Word_t api_version;
  L4_Word_t api_flags;
  L4_Word_t kernel_id;

  _L4_kernel_interface (&api_version, &api_flags, &kernel_id);

  return api_version;
}


/* Values for API flags field. */
#define L4_APIFLAG_LE		_L4_API_FLAGS_LITTLE_ENDIAN
#define L4_APIFLAG_BE		_L4_API_FLAGS_BIG_ENDIAN
#define L4_APIFLAG_32BIT	_L4_API_FLAGS_WORDSIZE_32
#define L4_APIFLAG_64BIT	_L4_API_FLAGS_WORDSIZE_64


/* Returns the API flags.  */
static inline L4_Word_t
_L4_attribute_always_inline
L4_ApiFlags (void)
{
  L4_Word_t api_version;
  L4_Word_t api_flags;
  L4_Word_t kernel_id;

  _L4_kernel_interface (&api_version, &api_flags, &kernel_id);

  return api_flags;
}


/* FIXME: The official interface defines them as ((id << 16) + subid)
   but that does not make any sense.  */
#define L4_KID_L4_486		\
  ((_L4_KERNEL_ID_L4_486 << 8) + _L4_KERNEL_SUBID_L4_486)
#define L4_KID_L4_PENTIUM	\
  ((_L4_KERNEL_ID_L4_PENTIUM << 8) + _L4_KERNEL_SUBID_L4_PENTIUM)
#define L4_KID_L4_X86		\
  ((_L4_KERNEL_ID_L4_X86 << 8) + _L4_KERNEL_SUBID_L4_X86)
#define L4_KID_L4_MIPS		\
  ((_L4_KERNEL_ID_L4_MIPS << 8) + _L4_KERNEL_SUBID_L4_MIPS)
#define L4_KID_L4_ALPHA		\
  ((_L4_KERNEL_ID_L4_ALPHA << 8) + _L4_KERNEL_SUBID_L4_ALPHA)
#define L4_KID_FIASCO		\
  ((_L4_KERNEL_ID_FIASCO << 8) + _L4_KERNEL_SUBID_FIASCO)
/* FIXME: These are even more wrong.  The names are not correct.  We
   provide corrected ones in addition to the buggy ones.  */
#define L4_KID_L4KA_X86		\
  ((_L4_KERNEL_ID_L4KA_HAZELNUT << 8) + _L4_KERNEL_SUBID_L4KA_HAZELNUT)
#define L4_KID_L4KA_ARM		\
  ((_L4_KERNEL_ID_L4KA_PISTACHIO << 8) + _L4_KERNEL_SUBID_L4KA_PISTACHIO)
#define L4_KID_L4KA_HAZELNUT	\
  ((_L4_KERNEL_ID_L4KA_HAZELNUT << 8) + _L4_KERNEL_SUBID_L4KA_HAZELNUT)
#define L4_KID_L4KA_PISTACHIO	\
  ((_L4_KERNEL_ID_L4KA_PISTACHIO << 8) + _L4_KERNEL_SUBID_L4KA_PISTACHIO)
#define L4_KID_L4KA_STRAWBERRY	\
  ((_L4_KERNEL_ID_L4KA_STRAWBERRY << 8) + _L4_KERNEL_SUBID_L4KA_STRAWBERRY)


/* Returns the kernel ID.  */
static inline L4_Word_t
_L4_attribute_always_inline
L4_KernelId (void)
{
  L4_Word_t api_version;
  L4_Word_t api_flags;
  L4_Word_t kernel_id;

  _L4_kernel_interface (&api_version, &api_flags, &kernel_id);

  return kernel_id;
}


/* Returns the generation date of the kernel in YEAR, MONTH and
   DAY.  */
static inline void
_L4_attribute_always_inline
L4_KernelGenDate (void *kip, L4_Word_t *year, L4_Word_t *month, L4_Word_t *day)
{
  _L4_kernel_gen_date (kip, year, month, day);
}


/* Returns the kernel version.  */
static inline L4_Word_t
_L4_attribute_always_inline
L4_KernelVersion (void *kip)
{
  _L4_kern_desc_t kern = _L4_kernel_desc (kip);

  return kern->version.raw;
}


/* Values for the kernel supplier field.  */
#if _L4_BYTE_ORDER == _L4_BIG_ENDIAN
#define _L4_KERNEL_SUPPLIER(x) \
  ((((char[])x)[0] << 24) | (((char[])x)[1] << 16) \
   | (((char[])x)[2] << 8) | (((char[])x)[3]))
#else
#define _L4_KERNEL_SUPPLIER(x) \
  ((((char[])x)[3] << 24) | (((char[])x)[2] << 16) \
   | (((char[])x)[1] << 8) | (((char[])x)[0]))
#endif

#define L4_SUPL_GMD	_L4_KERNEL_SUPPLIER (_L4_KERNEL_SUPPLIER_GMD)
#define L4_SUPL_IBM	_L4_KERNEL_SUPPLIER (_L4_KERNEL_SUPPLIER_IBM)
#define L4_SUPL_UNSW	_L4_KERNEL_SUPPLIER (_L4_KERNEL_SUPPLIER_UNSW)
/* FIXME: This one is missing in the official libl4.  We add it.  */
#define L4_SUPL_TUD	_L4_KERNEL_SUPPLIER (_L4_KERNEL_SUPPLIER_TUD)
#define L4_SUPL_UKA	_L4_KERNEL_SUPPLIER (_L4_KERNEL_SUPPLIER_UKA)


/* Returns the kernel supplier.  */
static inline L4_Word_t
_L4_attribute_always_inline
L4_KernelSupplier (void *kip)
{
  _L4_kern_desc_t kern = _L4_kernel_desc (kip);

  return kern->supplier;
}


/* Returns the number of processors.  */
static inline L4_Word_t
_L4_attribute_always_inline
L4_NumProcessors (void *kip)
{
  return _L4_num_processors (kip);
}


/* Returns the number of memory descriptors.  */
static inline L4_Word_t
_L4_attribute_always_inline
L4_NumMemoryDescriptors (void *kip)
{
  return _L4_num_memory_desc (kip);
}


/* Returns the page size mask.  */
static inline L4_Word_t
_L4_attribute_always_inline
L4_PageSizeMask (void *kip)
{
  return _L4_page_size_mask (kip);
}


/* Returns the page rights.  */
static inline L4_Word_t
_L4_attribute_always_inline
L4_PageRights (void *kip)
{
  return _L4_page_rights (kip);
}


/* Returns the thread ID bits.  */
static inline L4_Word_t
_L4_attribute_always_inline
L4_ThreadIdBits (void *kip)
{
  return _L4_thread_id_bits (kip);
}


/* Returns the thread ID system base.  */
static inline L4_Word_t
_L4_attribute_always_inline
L4_ThreadIdSystemBase (void *kip)
{
  return _L4_thread_system_base (kip);
}


/* Returns the thread ID user base.  */
static inline L4_Word_t
_L4_attribute_always_inline
L4_ThreadIdUserBase (void *kip)
{
  return _L4_thread_user_base (kip);
}


/* Returns the read precision.  */
static inline L4_Word_t
_L4_attribute_always_inline
L4_ReadPrecision (void *kip)
{
  return _L4_read_precision (kip);
}


/* Returns the schedule precision.  */
static inline L4_Word_t
_L4_attribute_always_inline
L4_SchedulePrecision (void *kip)
{
  return _L4_schedule_precision (kip);
}


/* Returns the UTCB area size.  */
static inline L4_Word_t
_L4_attribute_always_inline
L4_UtcbAreaSizeLog2 (void *kip)
{
  return _L4_utcb_area_size_log2 (kip);
}


/* Returns the alignment.  */
static inline L4_Word_t
_L4_attribute_always_inline
L4_UtcbAlignmentLog2 (void *kip)
{
  return _L4_utcb_alignment_log2 (kip);
}


/* Returns the UTCB size.  */
static inline L4_Word_t
_L4_attribute_always_inline
L4_UtcbSize (void *kip)
{
  return _L4_utcb_size (kip);
}


/* Returns the KIP area size.  */
static inline L4_Word_t
_L4_attribute_always_inline
L4_KipAreaSizeLog2 (void *kip)
{
  return _L4_kip_area_size_log2 (kip);
}


/* Returns the boot info field.  */
static inline L4_Word_t
_L4_attribute_always_inline
L4_BootInfo (void *kip)
{
  return _L4_boot_info (kip);
}


/* Returns the kernel version string.  */
static inline char *
_L4_attribute_always_inline
L4_KernelVersionString (void *kip)
{
  return _L4_kernel_version_string (kip);
}


/* Returns the NUMth kernel feature string.  */
static inline char *
_L4_attribute_always_inline
L4_Feature (void *kip, L4_Word_t num)
{
  return _L4_feature (kip, num);
}


/* Returns a pointer to the NUMth memory descriptor.  */
static inline L4_MemoryDesc_t *
_L4_attribute_always_inline
L4_MemoryDesc (void *kip, L4_Word_t num)
{
  return (L4_MemoryDesc_t *) _L4_memory_desc (kip, num);
}


/* Returns a pointer to the NUMth processor descriptor.  */
static inline L4_ProcDesc_t *
_L4_attribute_always_inline
L4_ProcDesc (void *kip, L4_Word_t num)
{
  return (L4_ProcDesc_t *) _L4_proc_desc (kip, num);
}


/* Support Functions.  */

#define L4_UndefinedMemoryType		((L4_Word_t) _L4_MEMDESC_UNDEFINED)
#define L4_ConventionalMemoryType	((L4_Word_t) _L4_MEMDESC_CONVENTIONAL)
#define L4_ReservedMemoryType		((L4_Word_t) _L4_MEMDESC_RESERVED)
#define L4_DedicatedMemoryType		((L4_Word_t) _L4_MEMDESC_DEDICATED)
#define L4_SharedMemoryType		((L4_Word_t) _L4_MEMDESC_SHARED)
#define L4_BootLoaderSpecificMemoryType	((L4_Word_t) _L4_MEMDESC_BOOTLOADER)
#define L4_ArchitectureSpecificMemoryType ((L4_Word_t) _L4_MEMDESC_ARCH)


static inline L4_Bool_t
_L4_attribute_always_inline
#if defined(__cplusplus)
L4_IsVirtual (L4_MemoryDesc_t *mem_desc)
#else
L4_IsMemoryDescVirtual (L4_MemoryDesc_t *mem_desc)
#endif
{
  return _L4_is_memory_desc_virtual ((_L4_memory_desc_t) mem_desc);
}


static inline L4_Word_t
_L4_attribute_always_inline
#if defined(__cplusplus)
L4_Type (L4_MemoryDesc_t *mem_desc)
#else
L4_MemoryDescType (L4_MemoryDesc_t *mem_desc)
#endif
{
  return _L4_memory_desc_type ((_L4_memory_desc_t) mem_desc);
}


static inline L4_Word_t
_L4_attribute_always_inline
#if defined(__cplusplus)
L4_Low (L4_MemoryDesc_t *mem_desc)
#else
L4_MemoryDescLow(L4_MemoryDesc_t *mem_desc)
#endif
{
  return _L4_memory_desc_low ((_L4_memory_desc_t) mem_desc);
}


static inline L4_Word_t
_L4_attribute_always_inline
#if defined(__cplusplus)
L4_High (L4_MemoryDesc_t *mem_desc)
#else
L4_MemoryDescHigh (L4_MemoryDesc_t *mem_desc)
#endif
{
  return _L4_memory_desc_high ((_L4_memory_desc_t) mem_desc);
}


static inline L4_Word_t
_L4_attribute_always_inline
#if defined(__cplusplus)
L4_ExternalFreq (L4_ProcDesc_t *proc_desc)
#else
L4_ProcDescExternalFreq (L4_ProcDesc_t *proc_desc)
#endif
{
  return _L4_proc_external_freq ((_L4_proc_desc_t) proc_desc);
}


static inline L4_Word_t
_L4_attribute_always_inline
#if defined(__cplusplus)
L4_InternalFreq (L4_ProcDesc_t *proc_desc)
#else
L4_ProcDescInternalFreq (L4_ProcDesc_t *proc_desc)
#endif
{
  return _L4_proc_internal_freq ((_L4_proc_desc_t) proc_desc);
}
