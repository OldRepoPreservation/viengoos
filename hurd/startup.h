/* startup.h - Interface for starting a new program.
   Copyright (C) 2004 Free Software Foundation, Inc.
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
#include <sys/types.h>

#include <l4/types.h>

#include <hurd/types.h>


/* The version of the startup data defined by this header file.  */
#define HURD_STARTUP_VERSION_MAJOR UINT16_C (0)
#define HURD_STARTUP_VERSION_MINOR UINT16_C (0)


/* The virtual address size of the startup fpage.  */
#define HURD_STARTUP_ADDR	((void *) (32*1024))
#define HURD_STARTUP_SIZE_LOG2	(15)
#define HURD_STARTUP_SIZE	(1 << HURD_STARTUP_SIZE_LOG2)


/* A single capability comes with the information in this struct.  */
struct hurd_startup_cap
{
  /* The server thread providing this capability.  */
  l4_thread_id_t server;

  /* The capability ID.  FIXME: Should actually be a box ID (or a
     union with a box ID).  */
  hurd_cap_id_t cap_id;
};


struct hurd_startup_map
{
  /* The container from which to map.  */
  struct hurd_startup_cap cont;
  
  /* Container offset and access permission (in the lower bits).  */
  l4_word_t offset;

  /* The intended load address of the fpage.  */
  void *vaddr;

  /* Size of the fpage.  */
  size_t size;
};


/* The actual startup data.  A pointer to this data will be passed on
   the stack to the startup code (without a return address), and to
   the main program (with a return address, i.e. normal calling
   conventions).  */
struct hurd_startup_data
{
  /* The version fields.  All versions with the same major version are
     compatible.  */
  unsigned short version_major;
  unsigned short version_minor;

  /* The argument vector.  */
  char *argz;
  size_t argz_len;

  /* The wortel thread and cap ID, or L4_NILTHREAD if this task does
     not have permission to access wortel.  */
  struct hurd_startup_cap wortel;

  /* The container of the executable binary image.  */
  struct hurd_startup_cap image;

  /* The memory map of the executable image.  FIXME: Later, this needs
     more distinction, so that only the startup and pager code will be
     mapped in initially, for dynamic paging.  */
  unsigned int mapc;
  struct hurd_startup_map *mapv;

  /* The program header.  */
  void *phdr;
  size_t phdr_len;

  /* The entry point.  */
  void *entry_point;

  /* The container of the startup code.  */
  struct hurd_startup_cap startup;
};


#endif	/* _HURD_STARTUP_H */
