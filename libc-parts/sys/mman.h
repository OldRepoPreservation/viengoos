/* Definitions for BSD-style memory management.
   Copyright (C) 1994-2000, 2003, 2004, 2005 Free Software Foundation, Inc.
   This file is part of the GNU C Library.

   The GNU C Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.

   The GNU C Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with the GNU C Library; if not, write to the Free
   Software Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
   02111-1307 USA.  */

#ifndef	_SYS_MMAN_H
#define	_SYS_MMAN_H	1

#include <features.h>
#include <sys/types.h>
#define __need_size_t
#include <stddef.h>

/* Protections are chosen from these bits, OR'd together.  The
   implementation does not necessarily support PROT_EXEC or PROT_WRITE
   without PROT_READ.  The only guarantees are that no writing will be
   allowed without PROT_WRITE and no access will be allowed for PROT_NONE. */

#define PROT_READ	0x1		/* Page can be read.  */
#define PROT_WRITE	0x2		/* Page can be written.  */
#define PROT_EXEC	0x4		/* Page can be executed.  */
#define PROT_NONE	0x0		/* Page can not be accessed.  */
#define PROT_GROWSDOWN	0x01000000	/* Extend change to start of
					   growsdown vma (mprotect only).  */
#define PROT_GROWSUP	0x02000000	/* Extend change to start of
					   growsup vma (mprotect only).  */

/* Sharing types (must choose one and only one of these).  */
#define MAP_SHARED	0x01		/* Share changes.  */
#define MAP_PRIVATE	0x02		/* Changes are private.  */
#ifdef __USE_MISC
# define MAP_TYPE	0x0f		/* Mask for type of mapping.  */
#endif

/* Other flags.  */
#define MAP_FIXED	0x10		/* Interpret addr exactly.  */
#ifdef __USE_MISC
# define MAP_FILE	0
# define MAP_ANONYMOUS	0x20		/* Don't use a file.  */
# define MAP_ANON	MAP_ANONYMOUS
#endif

/* These are Linux-specific.  */
#ifdef __USE_MISC
# define MAP_GROWSDOWN	0x00100		/* Stack-like segment.  */
# define MAP_DENYWRITE	0x00800		/* ETXTBSY */
# define MAP_EXECUTABLE	0x01000		/* Mark it as an executable.  */
# define MAP_LOCKED	0x02000		/* Lock the mapping.  */
# define MAP_NORESERVE	0x04000		/* Don't check for reservations.  */
# define MAP_POPULATE	0x08000		/* Populate (prefault) pagetables.  */
# define MAP_NONBLOCK	0x10000		/* Do not block on IO.  */
#endif

/* Flags to `msync'.  */
#define MS_ASYNC	1		/* Sync memory asynchronously.  */
#define MS_SYNC		4		/* Synchronous memory sync.  */
#define MS_INVALIDATE	2		/* Invalidate the caches.  */

/* Flags for `mlockall'.  */
#define MCL_CURRENT	1		/* Lock all currently mapped pages.  */
#define MCL_FUTURE	2		/* Lock all additions to address
					   space.  */

/* Flags for `mremap'.  */
#ifdef __USE_GNU
# define MREMAP_MAYMOVE	1
# define MREMAP_FIXED	2
#endif

/* Advice to `madvise'.  */
#ifdef __USE_BSD
# define MADV_NORMAL	 0	/* No further special treatment.  */
# define MADV_RANDOM	 1	/* Expect random page references.  */
# define MADV_SEQUENTIAL 2	/* Expect sequential page references.  */
# define MADV_WILLNEED	 3	/* Will need these pages.  */
# define MADV_DONTNEED	 4	/* Don't need these pages.  */
# define MADV_REMOVE	 9	/* Remove these pages and resources.  */
# define MADV_DONTFORK	 10	/* Do not inherit across fork.  */
# define MADV_DOFORK	 11	/* Do inherit across fork.  */
#endif

/* The POSIX people had to invent similar names for the same things.  */
#ifdef __USE_XOPEN2K
# define POSIX_MADV_NORMAL	0 /* No further special treatment.  */
# define POSIX_MADV_RANDOM	1 /* Expect random page references.  */
# define POSIX_MADV_SEQUENTIAL	2 /* Expect sequential page references.  */
# define POSIX_MADV_WILLNEED	3 /* Will need these pages.  */
# define POSIX_MADV_DONTNEED	4 /* Don't need these pages.  */
#endif
/* Return value of `mmap' in case of an error.  */
#define MAP_FAILED	((void *) -1)

__BEGIN_DECLS
/* Map addresses starting near ADDR and extending for LEN bytes.  from
   OFFSET into the file FD describes according to PROT and FLAGS.  If ADDR
   is nonzero, it is the desired mapping address.  If the MAP_FIXED bit is
   set in FLAGS, the mapping will be at ADDR exactly (which must be
   page-aligned); otherwise the system chooses a convenient nearby address.
   The return value is the actual mapping address chosen or MAP_FAILED
   for errors (in which case `errno' is set).  A successful `mmap' call
   deallocates any previous mapping for the affected region.  */

extern void *mmap (void *__addr, size_t __len, int __prot,
		   int __flags, int __fd, off_t __offset) __THROW;

/* Deallocate any mapping for the region starting at ADDR and extending LEN
   bytes.  Returns 0 if successful, -1 for errors (and sets errno).  */
extern int munmap (void *__addr, size_t __len) __THROW;

/* Change the memory protection of the region starting at ADDR and
   extending LEN bytes to PROT.  Returns 0 if successful, -1 for errors
   (and sets errno).  */
extern int mprotect (void *__addr, size_t __len, int __prot) __THROW;

/* Synchronize the region starting at ADDR and extending LEN bytes with the
   file it maps.  Filesystem operations on a file being mapped are
   unpredictable before this is done.  Flags are from the MS_* set.

   This function is a cancellation point and therefore not marked with
   __THROW.  */
extern int msync (void *__addr, size_t __len, int __flags);

#ifdef __USE_BSD
/* Advise the system about particular usage patterns the program follows
   for the region starting at ADDR and extending LEN bytes.  */
extern int madvise (void *__addr, size_t __len, int __advice) __THROW;
#endif
#ifdef __USE_XOPEN2K
/* This is the POSIX name for this function.  */
extern int posix_madvise (void *__addr, size_t __len, int __advice) __THROW;
#endif

/* Guarantee all whole pages mapped by the range [ADDR,ADDR+LEN) to
   be memory resident.  */
extern int mlock (__const void *__addr, size_t __len) __THROW;

/* Unlock whole pages previously mapped by the range [ADDR,ADDR+LEN).  */
extern int munlock (__const void *__addr, size_t __len) __THROW;

/* Cause all currently mapped pages of the process to be memory resident
   until unlocked by a call to the `munlockall', until the process exits,
   or until the process calls `execve'.  */
extern int mlockall (int __flags) __THROW;

/* All currently mapped pages of the process' address space become
   unlocked.  */
extern int munlockall (void) __THROW;

__END_DECLS

#endif	/* sys/mman.h */
