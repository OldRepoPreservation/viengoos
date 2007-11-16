/* memory.h - Basic memory management interface.
   Copyright (C) 2007 Free Software Foundation, Inc.
   Written by Neal H. Walfield <neal@gnu.org>.

   This file is part of the GNU Hurd.

   The GNU Hurd is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 3 of the
   License, or (at your option) any later version.

   The GNU Hurd is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see
   <http://www.gnu.org/licenses/>.  */

#ifndef RM_MEMORY_H
#define RM_MEMORY_H

#include <l4.h>

enum memory_reservation
  {
    /* Our binary, never freed.  */
    memory_reservation_self = 1,
    /* Memory used during the initialization.  */
    memory_reservation_init,
    /* Memory used by boot modules.  */
    memory_reservation_system_executable,
    /* Memory used by boot modules.  */
    memory_reservation_modules,
  };

/* Address of the first byte of the first frame.  */
extern l4_word_t first_frame;
/* Address of the first byte of the last frame.  */
extern l4_word_t last_frame;

/* Reserve the memory starting at byte START and ending at byte END
   with the reservation RESERVATION.  The memory is added to the free
   pool if and when the reservation expires.  Returns true on success.
   Otherwise false, if the reservation could not be completed because
   of an existing reservation.  */
extern bool memory_reserve (l4_word_t start, l4_word_t end,
			    enum memory_reservation reservation);

/* Print the reserved regions.  */
extern void memory_reserve_dump (void);

/* If there is reserved memory occuring on or after byte START and on
   or before byte END, return true and the first byte of the first
   contiguous region in *START_RESERVATION and the last byte in
   *END_RESERVATION.  Otherwise, false.  */
extern bool memory_is_reserved (l4_word_t start, l4_word_t end,
				l4_word_t *start_reservation,
				l4_word_t *end_reservation);

/* Cause the reservation RESERVATION to expire.  Add all memory with
   this reservation to the free pool.  */
extern void memory_reservation_clear (enum memory_reservation reservation);

/* Grab all the memory in the system.  */
extern void memory_grab (void);

/* Allocate a page of memory.  Returns NULL if there is no memory
   available.  */
extern l4_word_t memory_frame_allocate (void);

/* Return the frame starting at address ADDR to the free pool.  */
extern void memory_frame_free (l4_word_t addr);

#endif
