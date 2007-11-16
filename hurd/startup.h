/* startup.h - Interface for starting a new program.
   Copyright (C) 2004, 2007 Free Software Foundation, Inc.
   This file is part of the GNU Hurd.

   The GNU Hurd is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.

   The GNU Hurd is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with the GNU C Library; if not, write to the Free
   Software Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
   02111-1307 USA.  */

#ifndef _HURD_STARTUP_H
#define _HURD_STARTUP_H	1

#include <stdint.h>
#include <stddef.h>

#include <l4/types.h>

#include <hurd/types.h>
#include <hurd/addr.h>

/* The version of the startup data defined by this header file.  */
#define HURD_STARTUP_VERSION_MAJOR UINT16_C (0)
#define HURD_STARTUP_VERSION_MINOR UINT16_C (0)

/* Hurd object descriptors are used as a way to provide a map of its
   layout to a new task.  */
struct hurd_object_desc
{
  /* The object.  */
  addr_t object;

  /* If the object is not a folio, then:  */

  /* The location of the storage.  (addr_chop (STORAGE,
     FOLIO_OBJECTS_LOG2) => the folio.)  */
  addr_t storage;

  /* The type of the object (for convenience).  */
  unsigned char type;
};

/* A program's startup data.  When a program is started, the address
   is in the program's SP.  */
struct hurd_startup_data
{
  /* The version fields.  All versions with the same major version are
     compatible.  */
  unsigned short version_major;
  unsigned short version_minor;

  /* Startup flags.  */
  l4_word_t flags;

  /* The UTCB area of this task.  */
  l4_fpage_t utcb_area;

  /* The argument vector.  */
  char *argz;
  /* Absolute address in the data space.  */
  size_t argz_len;

  /* The environment vector.  */
  char *envz;
  /* Absolute address in the data space.  */
  size_t envz_len;

  /* Thread id of the resource manager.  */
  l4_thread_id_t rm;

  /* Slot in which a capability designating the task's primary
     activity is stored.  */
  addr_t activity;

  /* Slot in which a capability designating the task's first thread is
     stored.  */
  addr_t thread;

  struct hurd_object_desc *descs;
  int desc_count;

  /* The program header.  */
  void *phdr;
  size_t phdr_len;
};


#endif	/* _HURD_STARTUP_H */
