/* t-l4-kip.c - A test for the KIP related code in libl4.
   Copyright (C) 2005 Free Software Foundation, Inc.
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

/* This must be included before anything else.  */
#include "environment.h"


#include <l4/kip.h>


/* Test the various ways to get a pointer to the kernel interface
   page, and the magic bytes at the beginning.  */
void
test_magic ()
{
  /* This is our atom.  We can't check it except by looking at the
     magic, and we set it in our faked environment anyway.  */
  _L4_api_version_t api_version;
  _L4_api_flags_t api_flags;
  _L4_kernel_id_t kernel_id;
  _L4_kip_t kip = _L4_kernel_interface (&api_version, &api_flags, &kernel_id);

  /* Verify that our fake KIP is actually there.  */
  check ("[intern]", "if fake KIP is installed",

	 (kip->magic[0] == 'L' && kip->magic[1] == '4'
	  && kip->magic[2] == (char) 0xe6 && kip->magic[3] == 'K'),

	 "_L4_kernel_interface kip magic == %c%c%c%c != L4\xe6K\n",
	 kip->magic[0], kip->magic[1], kip->magic[2], kip->magic[3]);


  /* Verify the other values returned by _L4_kernel_interface.  This
     is basically an internal consistency check.  */
  check ("[intern]", "if fake API version matches KIP",
	 (api_version == kip->api_version.raw),
	 "_L4_kernel_interface api_version == 0x%x != 0x%x\n",
	 api_version, kip->api_version.raw);
  check ("[intern]", "if fake API flags matches KIP",
	 (api_flags == kip->api_flags.raw),
	 "_L4_kernel_interface api_flags == 0x%x != 0x%x\n",
	 api_flags, kip->api_flags.raw);
  check ("[intern]", "if fake kernel ID flags matches KIP",
	 (kernel_id == _L4_kernel_desc (kip)->id.raw),
	 "_L4_kernel_interface kernel_id == 0x%x != 0x%x\n",
	 kernel_id, _L4_kernel_desc (kip)->id.raw);

#ifdef _L4_INTERFACE_GNU
  {
    /* Some other ways to get the same information.  */
    l4_kip_t kip_gnu = l4_kip ();

    check ("[GNU]", "if l4_kip() returns the KIP",
	   (kip_gnu == kip),
	   "kip_gnu == %p != %p\n", kip_gnu, kip);
  }
#endif

#ifdef _L4_INTERFACE_L4
  {
    L4_Word_t api_version_l4;
    L4_Word_t api_flags_l4;
    L4_Word_t kernel_id_l4;

    void *kip_l4 = L4_KernelInterface (&api_version_l4, &api_flags_l4,
				       &kernel_id_l4);

    check ("[L4]", "if L4_KernelInterface returns the KIP",
	   (kip_l4 == kip),
	   "kip_l4 == %p != %p\n", kip_l4, kip);

    check ("[L4]", "if L4 API version matches KIP",
	   (api_version_l4 == api_version),
	   "api_version_l4 == 0x%x != 0x%x\n", api_version_l4, api_version);
    check ("[L4]", "if L4 API flags matches KIP",
	   (api_flags_l4 == api_flags),
	   "api_flags_l4 == 0x%x != 0x%x\n", api_flags_l4, api_flags);
    check ("[L4]", "if L4 kernel ID flags matches KIP",
	   (kernel_id_l4 == kernel_id),
	   "kip_l4 == 0x%x != 0x%x\n", kernel_id_l4, kernel_id);
  }
#endif
}


void
test (void)
{
  test_magic ();
}
