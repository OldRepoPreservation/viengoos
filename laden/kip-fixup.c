/* kip-fixup.c - Fixup the L4 KIP.
   Copyright (C) 2003, 2007, 2008 Free Software Foundation, Inc.
   Written by Marcus Brinkmann.

   This file is part of the GNU Hurd.

   The GNU Hurd is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 3, or (at
   your option) any later version.

   The GNU Hurd is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA. */

#include "laden.h"

void
kip_fixup (void)
{
  /* The KIP is 4k aligned, and somewhere within the kernel image.  */
  l4_kip_t kip = (l4_kip_t) ((kernel.low + 0xfff) & ~0xfff);
  l4_kip_t kip2;
  int nr;

  while ((l4_word_t) kip < kernel.high
	 && (kip->magic[0] != 'L' || kip->magic[1] != '4'
	     || kip->magic[2] != '\xe6' || kip->magic[3] != 'K'))
    kip = (l4_kip_t) (((l4_word_t) kip) + 0x1000);

  if ((l4_word_t) kip >= kernel.high)
    panic ("No KIP found in the kernel.");
  debug (1, "KIP found at address %p.", kip);

  kip2 = kip + 0x1000;
  while ((l4_word_t) kip2 < kernel.high
	 && (kip2->magic[0] != 'L' || kip2->magic[1] != '4'
	     || kip2->magic[2] != '\xe6' || kip2->magic[3] != 'K'))
    kip2 = (l4_kip_t) (((l4_word_t) kip2) + 0x1000);

  if ((l4_word_t) kip2 < kernel.high)
    panic ("More than one KIP found in kernel.");

  l4_api_version_t api_version = l4_api_version_from (kip);
  switch (api_version.version)
    {
    case L4_API_VERSION_X2:
      /* Booting an x2 kernel.  */
      debug (1, "Booting an x2 kernel.");
      break;

    default:
      panic ("Don't know how to boot kernel with API version %x.%x\n",
	     api_version.version, api_version.subversion);
    }

  /* Load the rootservers into the KIP.  */
  kip->sigma0 = sigma0;
  kip->sigma1 = sigma1;
  kip->rootserver = rootserver;
  /* FIXME: We should be able to specify the UTCB area for the
     rootserver here, but L4 lacks this feature.  */

  debug (1, "Sigma0: Low 0x%x, High 0x%x, IP 0x%x, SP 0x%x",
	 sigma0.low, sigma0.high, sigma0.ip, sigma0.sp);
  if (kip->sigma1.low)
    debug (1, "Sigma1: Low 0x%x, High 0x%x, IP 0x%x, SP 0x%x",
	   sigma1.low, sigma1.high, sigma1.ip, sigma1.sp);
  debug (1, "Root: Low 0x%x, High 0x%x, IP 0x%x, SP 0x%x",
	 rootserver.low, rootserver.high, rootserver.ip, rootserver.sp);

  /* Load the memory map into the KIP.  */

  /* Copy the memory map descriptors into the KIP.  */
  if (memory_map_size > kip->memory_info.nr)
    panic ("Memory map table in KIP is too small.");

  memcpy ((char *) (((l4_word_t) kip) + kip->memory_info.mem_desc_ptr),
	  (char *) memory_map,
	  sizeof (l4_memory_desc_t) * memory_map_size);

  kip->memory_info.nr = memory_map_size;
  for (nr = 0; nr < memory_map_size; nr++)
    debug (1, "Memory Map %i: Type %s(%i)/%i, Low 0x%llx, High 0x%llx",
	   nr + 1, l4_memory_desc_type_to_string (memory_map[nr].type),
	   memory_map[nr].type, memory_map[nr].subtype,
	   (unsigned long long) (memory_map[nr].low << 10),
	   (unsigned long long) (memory_map[nr].high << 10));

  /* Load the boot info into the KIP.  */
  kip->boot_info = boot_info;
}
