/* l4/compat/space.h - Public interface for L4 spaces.
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

#ifndef _L4_SPACE_H
# error "Never use <l4/compat/space.h> directly; include <l4/space.h> instead."
#endif


/* 2.1 ThreadID [Data Type]  */

/* Generic Programming Interface.  */

/* L4_Fpage_t is defined in <l4/compat/types.h>.  */

#define L4_Readable		_L4_readable
#define L4_Writable		_L4_writable
#define L4_eXecutable		_L4_executable
#define L4_FullyAccessible	_L4_fullly_accesible
#define L4_ReadeXecOnly		_L4_read_exec_only
#define L4_NoAccess		_L4_no_access

#define L4_Nilpage		((L4_Fpage_t) { .raw = _L4_nilpage })
#define L4_CompleteAddressSpace	\
  ((L4_Fpage_t) { .raw = _L4_complete_address_space })


static inline L4_Bool_t
_L4_attribute_always_inline
L4_IsNilFpage (L4_Fpage_t f)
{
  return _L4_is_nil_fpage (f.raw);
}


static inline L4_Fpage_t
_L4_attribute_always_inline
L4_Fpage (L4_Word_t BaseAddress, int FpageSize)
{
  L4_Fpage_t fpage;

  fpage.raw = _L4_fpage (BaseAddress, FpageSize);
  return fpage;
}


static inline L4_Fpage_t
_L4_attribute_always_inline
L4_FpageLog2 (L4_Word_t BaseAddress, int Log2FpageSize)
{
  L4_Fpage_t fpage;

  fpage.raw = _L4_fpage_log2 (BaseAddress, Log2FpageSize);
  return fpage;
}


static inline L4_Word_t
_L4_attribute_always_inline
L4_Address (L4_Fpage_t fpage)
{
  return _L4_address (fpage.raw);
}

static inline L4_Word_t
_L4_attribute_always_inline
L4_Size (L4_Fpage_t fpage)
{
  return _L4_size (fpage.raw);
}


static inline L4_Word_t
_L4_attribute_always_inline
L4_SizeLog2 (L4_Fpage_t fpage)
{
  return _L4_size_log2 (fpage.raw);
}


static inline L4_Word_t
_L4_attribute_always_inline
L4_Rights (L4_Fpage_t fpage)
{
  return _L4_rights (fpage.raw);
}


static inline void
_L4_attribute_always_inline
L4_Set_Rights (L4_Fpage_t *fpage, L4_Word_t AccessRights)
{
  _L4_set_rights (&fpage->raw, AccessRights);
}


static inline L4_Fpage_t
_L4_attribute_always_inline
#ifdef _cplusplus
operator + (const L4_Fpage_t l, const L4_Word_t r)
#else
L4_FpageAddRights (L4_Fpage_t l, L4_Word_t r)
#endif
{
  L4_Fpage_t new_fpage;

  new_fpage.raw = _L4_fpage_add_rights (l.raw, r);
  return new_fpage;
}


static inline L4_Fpage_t
_L4_attribute_always_inline
#ifdef _cplusplus
operator - (const L4_Fpage_t l, const L4_Word_t r)
#else
L4_FpageRemoveRights (const L4_Fpage_t l, const L4_Word_t r)
#endif
{
  L4_Fpage_t new_fpage;

  new_fpage.raw = _L4_fpage_remove_rights (l.raw, r);
  return new_fpage;
}


#ifdef _cplusplus

static inline L4_Fpage_t&
_L4_attribute_always_inline
operator += (L4_Fpage_t& l, const L4_Word_t r)
{
  _L4_fpage_add_rights_to (&l.raw, r);
  return l;
}


static inline L4_Fpage_t&
_L4_attribute_always_inline
operator -= (L4_Fpage_t& l, const L4_Word_t r)
{
  _L4_fpage_remove_rights_from (&l.raw, r);
  return l;
}

#else

static inline L4_Fpage_t *
_L4_attribute_always_inline
L4_FpageAddRightsTo (L4_Fpage_t *l, L4_Word_t r)
{
  _L4_fpage_add_rights_to (&l->raw, r);
  return l;
}


static inline L4_Fpage_t *
_L4_attribute_always_inline
L4_FpageRemoveRightsFrom (L4_Fpage_t *l, L4_Word_t r)
{
  _L4_fpage_remove_rights_from (&l->raw, r);
  return l;
}

#endif


/* 4.2 Unmap [Systemcall]  */

/* Generic Programming Interface.  */

static inline void
_L4_attribute_always_inline
L4_Unmap (L4_Word_t control)
{
  _L4_unmap (control);
}


/* Convenience Programming Interface.  */

static inline L4_Fpage_t
_L4_attribute_always_inline
#ifdef _cplusplus
L4_Unmap (L4_Fpage_t fpage)
#else
L4_UnmapFpage (L4_Fpage_t fpage)
#endif
{
  _L4_unmap_fpage (fpage.raw);
  return fpage;
}


static inline void
_L4_attribute_always_inline
#ifdef _cplusplus
L4_Unmap (L4_Word_t n, L4_Fpage_t *fpages, )
#else
L4_UnmapFpages (L4_Word_t n, L4_Fpage_t *fpages)
#endif
{
  _L4_unmap_fpages (n, &fpages[0].raw);
}


static inline L4_Fpage_t
_L4_attribute_always_inline
L4_Flush (L4_Fpage_t fpage)
{
  _L4_flush (fpage.raw);
  return fpage;
}


static inline void
_L4_attribute_always_inline
#ifdef _cplusplus
L4_Flush (L4_Word_t n, L4_Fpage_t *fpages, )
#else
L4_FlushFpage (L4_Word_t n, L4_Fpage_t *fpages)
#endif
{
  _L4_flush_fpages (n, &fpages[0].raw);
}


static inline L4_Fpage_t
_L4_attribute_always_inline
L4_GetStatus (L4_Fpage_t fpage)
{
  L4_Fpage_t status;

  status.raw = _L4_get_status (fpage.raw);
  return status;
}


static inline L4_Bool_t
_L4_attribute_always_inline
L4_WasReferenced (L4_Fpage_t fpage)
{
  return _L4_was_referenced (fpage.raw);
}


static inline L4_Bool_t
_L4_attribute_always_inline
L4_WasWritten (L4_Fpage_t fpage)
{
  return _L4_was_written (fpage.raw);
}


static inline L4_Bool_t
_L4_attribute_always_inline
L4_WaseXecuted (L4_Fpage_t fpage)
{
  return _L4_was_executed (fpage.raw);
}


/* 4.3 SpaceControl [Privileged Systemcall]  */

/* Generic Programming Interface.  */

static inline L4_Word_t
_L4_attribute_always_inline
L4_SpaceControl (L4_ThreadId_t SpaceSpecifier, L4_Word_t control,
		 L4_Fpage_t KipArea, L4_Fpage_t UtcbArea,
		 L4_ThreadId_t Redirector, L4_Word_t *old_control)
{
  return _L4_space_control (SpaceSpecifier.raw, control, KipArea.raw,
			    UtcbArea.raw, Redirector.raw, old_control);
}
