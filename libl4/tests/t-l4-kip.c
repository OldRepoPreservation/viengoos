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

#include <string.h>
#include <stddef.h>

/* This must be included before any libl4 header.  */
#include "environment.h"


#include <l4/kip.h>


/* Test the various ways to get a pointer to the kernel interface
   page, and the magic bytes at the beginning.  */
void *
test_magic ()
{
  void *kip_ok = (void *) environment_kip;
  word_t api_version_ok = environment_api_version;
  word_t api_flags_ok = environment_api_flags;
  word_t kernel_id_ok = environment_kernel_id;

#ifdef _L4_INTERFACE_INTERN
  {
    /* This is our atom.  We can't check it except by looking at the
       magic, and we set it in our faked environment anyway.  */
    _L4_api_version_t api_version;
    _L4_api_flags_t api_flags;
    _L4_kernel_id_t kernel_id;
    _L4_kip_t kip = _L4_kernel_interface (&api_version, &api_flags,
					  &kernel_id);

    check ("[GNU]", "if l4_kip() returns the KIP",
	   (kip == kip_ok),
	   "kip == %p != %p\n", kip, kip_ok);

    /* Verify that our fake KIP is actually there.  */
    check ("[intern]", "if fake KIP is installed",
	   (kip->magic[0] == 'L' && kip->magic[1] == '4'
	    && kip->magic[2] == (char) 0xe6 && kip->magic[3] == 'K'),
	   "_L4_kernel_interface kip magic == %c%c%c%c != L4\xe6K\n",
	   kip->magic[0], kip->magic[1], kip->magic[2], kip->magic[3]);
 
    /* Verify the other values returned by _L4_kernel_interface.  This
       is basically an internal consistency check.  */
    check_nr ("[intern]", "if fake API version is returned",
	      api_version, api_version_ok);
    check_nr ("[intern]", "if fake API version matches KIP",
	      api_version, kip->api_version.raw);
    check_nr ("[intern]", "if fake API flags are returned",
	      api_flags, api_flags_ok);
    check_nr ("[intern]", "if fake API flags matches KIP",
	      api_flags, kip->api_flags.raw);
    check_nr ("[intern]", "if fake kernel ID is returned",
	      kernel_id, kernel_id_ok);
    check_nr ("[intern]", "if fake kernel ID matches KIP",
	      kernel_id, _L4_kernel_desc (kip)->id.raw);
  }
#endif

#ifdef _L4_INTERFACE_GNU
  {
    /* Some other ways to get the same information.  */
    l4_kip_t kip = l4_kip ();

    check ("[GNU]", "if l4_kip() returns the KIP",
	   (kip == kip_ok),
	   "kip == %p != %p\n", kip, kip_ok);
  }
#endif

#ifdef _L4_INTERFACE_L4
  {
    L4_Word_t api_version;
    L4_Word_t api_flags;
    L4_Word_t kernel_id;
    void *kip = L4_KernelInterface (&api_version, &api_flags, &kernel_id);

    check ("[L4]", "if L4_KernelInterface returns the KIP",
	   (kip == kip_ok),
	   "kip == %p != %p\n", kip, kip);

    check_nr ("[L4]", "if L4 API version matches KIP",
	      api_version, api_version_ok);
    check_nr ("[L4]", "if L4 API flags matches KIP",
	      api_flags, api_flags_ok);
    check_nr ("[L4]", "if L4 kernel ID flags matches KIP",
	      kernel_id, kernel_id_ok);

    kip = L4_GetKernelInterface ();
    check ("[L4]", "L4_GetKernelInterface",
	   (kip == kip),
	   "kip == %p != %p\n", kip, kip);
  }
#endif

  return kip_ok;
}


/* Test the various ways to get info about the API version, API flags
   and Kernel Id.  */
void
test_api_and_kernel_id ()
{
#ifdef _L4_INTERFACE_INTERN
  _L4_api_version_t api_version;
  _L4_api_flags_t api_flags;
  _L4_kernel_id_t kernel_id;
  _L4_kip_t kip = _L4_kernel_interface (&api_version, &api_flags, &kernel_id);

  check_nr ("[intern]", "_L4_api_version", _L4_api_version (kip), api_version);
  check_nr ("[intern]", "_L4_api_flags", _L4_api_flags (kip), api_flags);
  check_nr ("[intern]", "_L4_kernel_id", _L4_kernel_id (kip), kernel_id);

  check_nr ("[intern]", "_L4_API_VERSION_2", _L4_API_VERSION_2, 0x02);
  check_nr ("[intern]", "_L4_API_VERSION_X0", _L4_API_VERSION_X0, 0x83);
  check_nr ("[intern]", "_L4_API_SUBVERSION_X0", _L4_API_SUBVERSION_X0, 0x80);
  check_nr ("[intern]", "_L4_API_VERSION_X1", _L4_API_VERSION_X1, 0x83);
  check_nr ("[intern]", "_L4_API_SUBVERSION_X1", _L4_API_SUBVERSION_X1, 0x81);
  check_nr ("[intern]", "_L4_API_VERSION_X2", _L4_API_VERSION_X2, 0x84);
  check_nr ("[intern]", "_L4_API_VERSION_4", _L4_API_VERSION_4, 0x04);

  {
    _L4_word_t api_version_version_good = _L4_API_VERSION_X2;
    _L4_word_t api_version_subversion_good = 0x05;	/* rev */
    __L4_api_version_t api_version_s = { .raw = api_version };

    check_nr ("[intern]", "API version",
	      api_version_s.version, api_version_version_good);
    check_nr ("[intern]", "API subversion",
	      api_version_s.subversion, api_version_subversion_good);
  }

  check_nr ("[intern]", "_L4_API_FLAGS_WORDSIZE_32",
	    _L4_API_FLAGS_WORDSIZE_32, 0x00);
  check_nr ("[intern]", "_L4_API_FLAGS_WORDSIZE_64",
	    _L4_API_FLAGS_WORDSIZE_64, 0x01);

  {
    _L4_word_t api_flags_endian = 0x00;		/* Little Endian */
    _L4_word_t api_flags_wordsize = 0x00;	/* 32-bit */
    __L4_api_flags_t api_flags_s = { .raw = api_flags };

    check_nr ("[intern]", "API flags endianess",
	      api_flags_s.endian, api_flags_endian);
    check_nr ("[intern]", "API flags wordsize",
	      api_flags_s.wordsize, api_flags_wordsize);
  }

  check_nr ("[intern]", "_L4_KERNEL_ID_L4_486",
	    _L4_KERNEL_ID_L4_486, 0x00);
  check_nr ("[intern]", "_L4_KERNEL_SUBID_L4_486",
	    _L4_KERNEL_SUBID_L4_486, 0x01);
  check_nr ("[intern]", "_L4_KERNEL_ID_L4_PENTIUM",
	    _L4_KERNEL_ID_L4_PENTIUM, 0x00);
  check_nr ("[intern]", "_L4_KERNEL_SUBID_L4_PENTIUM",
	    _L4_KERNEL_SUBID_L4_PENTIUM, 0x02);
  check_nr ("[intern]", "_L4_KERNEL_ID_L4_X86",
	    _L4_KERNEL_ID_L4_X86, 0x00);
  check_nr ("[intern]", "_L4_KERNEL_SUBID_L4_X86",
	    _L4_KERNEL_SUBID_L4_X86, 0x03);
  check_nr ("[intern]", "_L4_KERNEL_ID_L4_MIPS",
	    _L4_KERNEL_ID_L4_MIPS, 0x01);
  check_nr ("[intern]", "_L4_KERNEL_SUBID_L4_MIPS",
	    _L4_KERNEL_SUBID_L4_MIPS, 0x01);
  check_nr ("[intern]", "_L4_KERNEL_ID_L4_ALPHA",
	    _L4_KERNEL_ID_L4_ALPHA, 0x02);
  check_nr ("[intern]", "_L4_KERNEL_SUBID_L4_ALPHA",
	    _L4_KERNEL_SUBID_L4_ALPHA, 0x01);
  check_nr ("[intern]", "_L4_KERNEL_ID_FIASCO",
	    _L4_KERNEL_ID_FIASCO, 0x03);
  check_nr ("[intern]", "_L4_KERNEL_SUBID_FIASCO",
	    _L4_KERNEL_SUBID_FIASCO, 0x01);
  check_nr ("[intern]", "_L4_KERNEL_ID_L4KA_HAZELNUT",
	    _L4_KERNEL_ID_L4KA_HAZELNUT, 0x04);
  check_nr ("[intern]", "_L4_KERNEL_SUBID_L4KA_HAZELNUT",
	    _L4_KERNEL_SUBID_L4KA_HAZELNUT, 0x01);
  check_nr ("[intern]", "_L4_KERNEL_ID_L4KA_PISTACHIO",
	    _L4_KERNEL_ID_L4KA_PISTACHIO, 0x04);
  check_nr ("[intern]", "_L4_KERNEL_SUBID_L4KA_PISTACHIO",
	    _L4_KERNEL_SUBID_L4KA_PISTACHIO, 0x02);
  check_nr ("[intern]", "_L4_KERNEL_ID_L4KA_STRAWBERRY",
	    _L4_KERNEL_ID_L4KA_STRAWBERRY, 0x04);
  check_nr ("[intern]", "_L4_KERNEL_SUBID_L4KA_STRAWBERRY",
	    _L4_KERNEL_SUBID_L4KA_STRAWBERRY, 0x03);
#endif

#ifdef _L4_INTERFACE_L4
  {
    L4_Word_t api_version_l4;
    L4_Word_t api_flags_l4;
    L4_Word_t kernel_id_l4;

    (void) L4_KernelInterface (&api_version_l4, &api_flags_l4, &kernel_id_l4);

    check_nr ("[L4]", "L4_ApiVersion", L4_ApiVersion (), api_version_l4);
    check_nr ("[L4]", "L4_ApiFlags", L4_ApiFlags (), api_flags_l4);
    check_nr ("[L4]", "L4_KernelId", L4_KernelId (), kernel_id_l4);
  }

  check_nr ("[L4]", "L4_APIVERSION_2",
	    L4_APIVERSION_2, _L4_API_VERSION_2);
  check_nr ("[L4]", "L4_APIVERSION_X0",
	    L4_APIVERSION_X0, _L4_API_VERSION_X0);
  check_nr ("[L4]", "L4_APISUBVERSION_X0",
	    L4_APISUBVERSION_X0, _L4_API_SUBVERSION_X0);
  check_nr ("[L4]", "L4_APIVERSION_X1",
	    L4_APIVERSION_X1, _L4_API_VERSION_X1);
  check_nr ("[L4]", "L4_APISUBVERSION_X1",
	    L4_APISUBVERSION_X1, _L4_API_SUBVERSION_X1);
  check_nr ("[L4]", "L4_APIVERSION_X2",
	    L4_APIVERSION_X2, _L4_API_VERSION_X2);
  check_nr ("[L4]", "L4_APISUBVERSION_X2",
	    L4_APISUBVERSION_X2, 0x82);

  check_nr ("[L4]", "L4_APIFLAG_32BIT",
	    L4_APIFLAG_32BIT, _L4_API_FLAGS_WORDSIZE_32);
  check_nr ("[L4]", "L4_APIFLAG_64BIT",
	    L4_APIFLAG_64BIT, _L4_API_FLAGS_WORDSIZE_64);

  check_nr ("[L4]", "L4_KID_L4_486", L4_KID_L4_486, 0x0001);
  check_nr ("[L4]", "L4_KID_L4_PENTIUM", L4_KID_L4_PENTIUM, 0x0002);
  check_nr ("[L4]", "L4_KID_L4_X86", L4_KID_L4_X86, 0x0003);
  check_nr ("[L4]", "L4_KID_L4_MIPS", L4_KID_L4_MIPS, 0x0101);
  check_nr ("[L4]", "L4_KID_L4_ALPHA", L4_KID_L4_ALPHA, 0x0201);
  check_nr ("[L4]", "L4_KID_FIASCO", L4_KID_FIASCO, 0x0301);
  check_nr ("[L4]", "L4_KID_L4KA_HAZELNUT", L4_KID_L4KA_HAZELNUT, 0x0401);
  check_nr ("[L4]", "L4_KID_L4KA_PISTACHIO", L4_KID_L4KA_PISTACHIO, 0x0402);
  check_nr ("[L4]", "L4_KID_L4KA_STRAWBERRY", L4_KID_L4KA_STRAWBERRY, 0x0403);
#endif
}


/* Test the kernel generation date.  */
void
test_kernel_gen_date (_L4_kip_t kip)
{
  word_t year_ok = 2005;
  word_t month_ok = 1;
  word_t day_ok = 22;

  word_t year;
  word_t month;
  word_t day;

#ifdef _L4_INTERFACE_INTERN
  _L4_kernel_gen_date (kip, &year, &month, &day);
  
  check ("[intern]", "_L4_kernel_gen_date",
	 (year == year_ok && month == month_ok && day == day_ok),
	 "_L4_kernel_gen_date == %d/%d/%d != %d/%d/%d",
	 year, month, day, year_ok, month_ok, day_ok);
#endif

#ifdef _L4_INTERFACE_GNU
  l4_kernel_gen_date (&year, &month, &day);

  check ("[GNU]", "l4_kernel_gen_date",
	 (year == year_ok && month == month_ok && day == day_ok),
	 "l4_kernel_gen_date == %d/%d/%d != %d/%d/%d",
	 year, month, day, year_ok, month_ok, day_ok);

  l4_kernel_gen_date_from (kip, &year, &month, &day);

  check ("[GNU]", "l4_kernel_gen_date_from",
	 (year == year_ok && month == month_ok && day == day_ok),
	 "l4_kernel_gen_date_from == %d/%d/%d != %d/%d/%d",
	 year, month, day, year_ok, month_ok, day_ok);
#endif

#ifdef _L4_INTERFACE_L4
  L4_KernelGenDate (kip, &year, &month, &day);

  check ("[L4]", "L4_KernelGenDate",
	 (year == year_ok && month == month_ok && day == day_ok),
	 "L4_KernelGenDate == %d/%d/%d != %d/%d/%d",
	 year, month, day, year_ok, month_ok, day_ok);
#endif
}


/* Test the kernel version.  */
void
test_kernel_version (_L4_kip_t kip)
{
  word_t version_ok = 0;
  word_t subversion_ok = 4;
  word_t subsubversion_ok = 0;

#ifdef _L4_INTERFACE_INTERN
  {
    word_t version;
    word_t subversion;
    word_t subsubversion;

    _L4_kernel_version (kip, &version, &subversion, &subsubversion);
  
    check ("[intern]", "_L4_kernel_version",
	   (version == version_ok && subversion == subversion_ok
	    && subsubversion == subsubversion_ok),
	   "_L4_kernel_version == %d.%d.%d != %d.%d.%d",
	   version, subversion, subsubversion,
	   version_ok, subversion_ok, subsubversion_ok);
  }
#endif

#ifdef _L4_INTERFACE_GNU
  {
    word_t version;
    word_t subversion;
    word_t subsubversion;

    l4_kernel_version (&version, &subversion, &subsubversion);

    check ("[GNU]", "l4_kernel_version",
	   (version == version_ok && subversion == subversion_ok
	    && subsubversion == subsubversion_ok),
	   "l4_kernel_version == %d.%d.%d != %d.%d.%d",
	   version, subversion, subsubversion, version_ok,
	   subversion_ok, subsubversion_ok);

    l4_kernel_version_from (kip, &version, &subversion, &subsubversion);

    check ("[GNU]", "l4_kernel_version_from",
	   (version == version_ok && subversion == subversion_ok
	    && subsubversion == subsubversion_ok),
	   "l4_kernel_version_from == %d.%d.%d != %d.%d.%d",
	   version, subversion, subsubversion, version_ok,
	   subversion_ok, subsubversion_ok);
  }
#endif

#ifdef _L4_INTERFACE_L4
  {
    word_t full_version_ok = (version_ok << 24)
      | (subversion_ok << 16) | subsubversion_ok;
    word_t version;

    version = L4_KernelVersion (kip);

    check ("[L4]", "L4_KernelVersion",
	   (version == full_version_ok),
	   "L4_KernelVersion == 0x%x != 0x%x", version, full_version_ok);
  }
#endif
}


/* Test the kernel supplier.  */
void
test_kernel_supplier (_L4_kip_t kip)
{
#ifdef _L4_INTERFACE_INTERN
  {
    const char supplier_ok[] = _L4_KERNEL_SUPPLIER_UKA;
    char *supplier;

    supplier = (char[]) _L4_KERNEL_SUPPLIER_GMD;
    check ("[intern]", "_L4_KERNEL_SUPPLIER_GMD",
	   (!strncmp (supplier, "GMD ", 4)),
	   "_L4_KERNEL_SUPPLIER_GMD == '%.4s' != 'GMD '", supplier);
    supplier = (char[]) _L4_KERNEL_SUPPLIER_IBM;
    check ("[intern]", "_L4_KERNEL_SUPPLIER_IBM",
	   (!strncmp (supplier, "IBM ", 4)),
	   "_L4_KERNEL_SUPPLIER_IBM == '%.4s' != 'IBM '", supplier);
    supplier = (char[]) _L4_KERNEL_SUPPLIER_UNSW;
    check ("[intern]", "_L4_KERNEL_SUPPLIER_UNSW",
	   (!strncmp (supplier, "UNSW", 4)),
	   "_L4_KERNEL_SUPPLIER_UNSW == '%.4s' != 'UNSW'", supplier);
    supplier = (char[]) _L4_KERNEL_SUPPLIER_TUD;
    check ("[intern]", "_L4_KERNEL_SUPPLIER_TUD",
	   (!strncmp (supplier, "TUD ", 4)),
	   "_L4_KERNEL_SUPPLIER_TUD == '%.4s' != 'TUD '", supplier);
    supplier = (char[]) _L4_KERNEL_SUPPLIER_UKA;
    check ("[intern]", "_L4_KERNEL_SUPPLIER_UKA",
	   (!strncmp (supplier, "UKa ", 4)),
	   "_L4_KERNEL_SUPPLIER_UKA == '%.4s' != 'UKa '", supplier);

    supplier = _L4_kernel_supplier (kip);
    check ("[intern]", "_L4_kernel_supplier",
	   (!strncmp (supplier, supplier_ok, sizeof (supplier_ok))),
	   "_L4_kernel_supplier == '%.4s' != '%.4s'", supplier, supplier_ok);
  }
#endif

#ifdef _L4_INTERFACE_GNU
  {
    const char supplier_ok[] = _L4_KERNEL_SUPPLIER_UKA;
    char *supplier;

    check ("[GNU]", "L4_KERNEL_SUPPLIER_GMD",
	   (!strncmp (((char[]) L4_KERNEL_SUPPLIER_GMD),
		      ((char[]) _L4_KERNEL_SUPPLIER_GMD), 4)),
	   "L4_KERNEL_SUPPLIER_GMD == '%.4s' != '%.4s'",
	   (char[]) L4_KERNEL_SUPPLIER_GMD, (char[]) _L4_KERNEL_SUPPLIER_GMD);
    check ("[GNU]", "L4_KERNEL_SUPPLIER_IBM",
	   (!strncmp (((char[]) L4_KERNEL_SUPPLIER_IBM),
		      ((char[]) _L4_KERNEL_SUPPLIER_IBM), 4)),
	   "L4_KERNEL_SUPPLIER_IBM == '%.4s' != '%.4s'",
	   (char[]) L4_KERNEL_SUPPLIER_IBM, (char[]) _L4_KERNEL_SUPPLIER_IBM);
    check ("[GNU]", "L4_KERNEL_SUPPLIER_UNSW",
	   (!strncmp (((char[]) L4_KERNEL_SUPPLIER_UNSW),
		      ((char[]) _L4_KERNEL_SUPPLIER_UNSW), 4)),
	   "L4_KERNEL_SUPPLIER_UNSW == '%.4s' != '%.4s'",
	   (char[]) L4_KERNEL_SUPPLIER_UNSW,
	   (char[]) _L4_KERNEL_SUPPLIER_UNSW);
    check ("[GNU]", "L4_KERNEL_SUPPLIER_TUD",
	   (!strncmp (((char[]) L4_KERNEL_SUPPLIER_TUD),
		      ((char[]) _L4_KERNEL_SUPPLIER_TUD), 4)),
	   "L4_KERNEL_SUPPLIER_TUD == '%.4s' != '%.4s'",
	   (char[]) L4_KERNEL_SUPPLIER_TUD, (char[]) _L4_KERNEL_SUPPLIER_TUD);
    check ("[GNU]", "L4_KERNEL_SUPPLIER_UKA",
	   (!strncmp (((char[]) L4_KERNEL_SUPPLIER_UKA),
		      ((char[]) _L4_KERNEL_SUPPLIER_UKA), 4)),
	   "L4_KERNEL_SUPPLIER_UKA == '%.4s' != '%.4s'",
	   (char[]) L4_KERNEL_SUPPLIER_UKA, (char[]) _L4_KERNEL_SUPPLIER_UKA);

    supplier = l4_kernel_supplier_from (kip);
    check ("[GNU]", "l4_kernel_supplier_from",
	   (!strncmp (supplier, supplier_ok, sizeof (supplier_ok))),
	   "l4_kernel_supplier_from == '%.4s' != '%.4s'",
	   supplier, supplier_ok);

    supplier = l4_kernel_supplier ();
    check ("[GNU]", "l4_kernel_supplier",
	   (!strncmp (supplier, supplier_ok, sizeof (supplier_ok))),
	   "l4_kernel_supplier == '%.4s' != '%.4s'", supplier, supplier_ok);
  }
#endif

#ifdef _L4_INTERFACE_L4
  {
    L4_Word_t supl_ok;
    L4_Word_t supl;

    supl_ok = *((unsigned int *) ((char[]) _L4_KERNEL_SUPPLIER_GMD));
    check_nr ("[L4]", "L4_SUPL_GMD", L4_SUPL_GMD, supl_ok);
    supl_ok = *((unsigned int *) ((char[]) _L4_KERNEL_SUPPLIER_IBM));
    check_nr ("[L4]", "L4_SUPL_IBM", L4_SUPL_IBM, supl_ok);
    supl_ok = *((unsigned int *) ((char[]) _L4_KERNEL_SUPPLIER_UNSW));
    check_nr ("[L4]", "L4_SUPL_UNSW", L4_SUPL_UNSW, supl_ok);
    supl_ok = *((unsigned int *) ((char[]) _L4_KERNEL_SUPPLIER_TUD));
    check_nr ("[L4]", "L4_SUPL_TUD", L4_SUPL_TUD, supl_ok);
    supl_ok = *((unsigned int *) ((char[]) _L4_KERNEL_SUPPLIER_UKA));
    check_nr ("[L4]", "L4_SUPL_UKA", L4_SUPL_UKA, supl_ok);

    supl_ok = *((unsigned int *) ((char[]) _L4_KERNEL_SUPPLIER_UKA));
    supl = L4_KernelSupplier (kip);
    check ("[L4]", "L4_KernelSupplier",
	   (supl == supl_ok),
	   "L4_KernelSupplier == 0x%x != 0x%x", supl, supl_ok);
  }
#endif
}  


/* Test the kernel version string.  */
void
test_kernel_version_string (_L4_kip_t kip)
{
  const char version_string_ok[] = "L4Ka::Pistachio - "
    "built on Jan 22 2005 02:26:14 by marcus@ulysses "
    "using gcc version 3.3.4 (Debian 1:3.3.4-13)";
  char *version_string;

#ifdef _L4_INTERFACE_INTERN
  version_string = _L4_kernel_version_string (kip);
  check ("[intern]", "_L4_kernel_version_string",
	 (!strcmp (version_string, version_string_ok)),
	 "_L4_kernel_version_string == '%s' != '%s'",
	 version_string, version_string_ok);
#endif

#ifdef _L4_INTERFACE_GNU
  version_string = l4_kernel_version_string_from (kip);
  check ("[GNU]", "l4_kernel_version_string_from",
	 (!strcmp (version_string, version_string_ok)),
	 "l4_kernel_version_string_from == '%s' != '%s'",
	 version_string, version_string_ok);

  version_string = l4_kernel_version_string ();
  check ("[GNU]", "l4_kernel_version_string",
	 (!strcmp (version_string, version_string_ok)),
	 "l4_kernel_version_string == '%s' != '%s'",
	 version_string, version_string_ok);
#endif

#ifdef _L4_INTERFACE_L4
  version_string = L4_KernelVersionString (kip);
  check ("[L4]", "L4_KernelVersionString",
	 (!strcmp (version_string, version_string_ok)),
	 "L4_KernelVersionString == '%s' != '%s'",
	 version_string, version_string_ok);
#endif
}


/* Test the kernel feature list.  */
void
test_kernel_feature (_L4_kip_t kip)
{
  const char *feature_ok[] = { "smallspaces", NULL };

  char *feature;
  word_t num = 0;
  int last_seen = 0;

  do
    {
      /* FIXME: Support [!_L4_INTERFACE_INTERN] (by code duplication?).  */
      feature = _L4_feature (kip, num);

      if (!last_seen && feature_ok[num] && feature)
	check ("[intern]", "_L4_feature",
	       (!strcmp (feature, feature_ok[num])),
	       "_L4_feature (kip, %i) == '%s' != '%s'",
	       num, feature, feature_ok[num]);
      else if (feature)
	{
	  check ("[intern]", "_L4_feature",
		 (feature == NULL),
		 "_L4_feature (kip, %i) == '%s' != NULL",
		 num, feature);
	  last_seen = 1;
	}

#ifdef _L4_INTERFACE_GNU
      check ("[GNU]", "l4_feature_from",
	     (l4_feature_from (kip, num) == feature),
	     "l4_feature_from (kip, %i) == %p != %p",
	     num, l4_feature_from (kip, num), feature);
      check ("[GNU]", "l4_feature",
	     (l4_feature (num) == feature),
	     "l4_feature (%i) == %p != %p",
	     num, l4_feature (num), feature);
#endif

#ifdef _L4_INTERFACE_L4
      check ("[L4]", "L4_Feature",
	     (L4_Feature (kip, num) == feature),
	     "L4_Feature (kip, %i) == %p != %p",
	     num, L4_Feature (kip, num), feature);
#endif

      num++;
    }
  while (feature);
}


/* Test the processor info field.  */
void
test_processor_info (_L4_kip_t kip)
{
  word_t processors_ok = 2;
  word_t internal_freq_ok[] = { 2809310, 2809311 };
  word_t external_freq_ok[] = { 0, 0 };
  word_t proc_desc_size_ok = 16;
  word_t num;
  _L4_proc_desc_t proc_desc_prev = 0;

#ifdef _L4_INTERFACE_INTERN
  check_nr ("[intern]", "_L4_num_processors",
	    _L4_num_processors (kip), processors_ok);
#endif

#ifdef _L4_INTERFACE_GNU
  check_nr ("[GNU]", "l4_num_processors_from",
	    l4_num_processors_from (kip), processors_ok);
  check_nr ("[GNU]", "l4_num_processors",
	    l4_num_processors (), processors_ok);
#endif

#ifdef _L4_INTERFACE_L4
  {
    volatile L4_ProcDesc_t proc_desc;
    volatile L4_Word_t raw;

    raw = proc_desc.raw[0];
    raw = proc_desc.raw[1];
    raw = proc_desc.raw[2];
    raw = proc_desc.raw[3];
  }

  check_nr ("[L4]", "L4_NumProcessors",
	    L4_NumProcessors (kip), processors_ok);
#endif

#ifdef _L4_INTERFACE_INTERN
  check_nr ("[intern]", "processor_info.log2_size",
	    (1 << kip->processor_info.log2_size), proc_desc_size_ok);
#endif

  for (num = 0; num < _L4_num_processors (kip); num++)
    {
      _L4_proc_desc_t proc_desc;

      /* FIXME: Support [!_L4_INTERFACE_INTERN] (by code duplication?).  */
      proc_desc = _L4_proc_desc (kip, num);
      check ("[intern]", "_L4_proc_desc (once per cpu)",
	     (proc_desc != NULL),
	     "_L4_proc_desc (kip, %i) == NULL != not null", num);

      check ("[intern]", "_L4_proc_internal_freq",
	     (_L4_proc_internal_freq (proc_desc) == internal_freq_ok[num]),
	     "_L4_proc_internal_freq (%i) == %i != %i",
	     num, _L4_proc_internal_freq (proc_desc), internal_freq_ok[num]);
      check ("[intern]", "_L4_proc_external_freq",
	     (_L4_proc_external_freq (proc_desc) == external_freq_ok[num]),
	     "_L4_proc_external_freq (%i) == %i != %i",
	     num, _L4_proc_external_freq (proc_desc), external_freq_ok[num]);

#ifdef _L4_INTERFACE_GNU
      {
	l4_proc_desc_t *proc_desc_gnu = l4_proc_desc_from (kip, num);
	
	check ("[GNU]", "l4_proc_desc_from",
	       (proc_desc_gnu == proc_desc),
	       "l4_proc_desc_from (kip, %i) == %p != %p",
	       num, proc_desc_gnu, proc_desc);
	check ("[GNU]", "l4_proc_desc",
	       (l4_proc_desc (num) == proc_desc),
	       "l4_proc_desc (%i) == %p != %p",
	       num, l4_proc_desc (num), proc_desc);
	
	check ("[GNU]", "l4_proc_internal_freq",
	       (l4_proc_internal_freq (proc_desc_gnu)
		== internal_freq_ok[num]),
	       "l4_proc_internal_freq (%i) == %i != %i",
	       num, l4_proc_internal_freq (proc_desc_gnu),
	       internal_freq_ok[num]);
	check ("[GNU]", "l4_proc_external_freq",
	       (l4_proc_external_freq (proc_desc_gnu)
		== external_freq_ok[num]),
	       "l4_proc_external_freq (%i) == %i != %i",
	       num, l4_proc_external_freq (proc_desc_gnu),
	       external_freq_ok[num]);
      }
#endif


#ifdef _L4_INTERFACE_L4
      {
	L4_ProcDesc_t *proc_desc_l4 = L4_ProcDesc (kip, num);

	check ("[L4]", "L4_ProcDesc",
	       ((void *) proc_desc_l4 == (void *) proc_desc),
	       "L4_ProcDesc (kip, %i) == %p != %p",
	       num, proc_desc_l4, proc_desc);

	check ("[L4]", "L4_ProcDescInternalFreq",
	       (L4_ProcDescInternalFreq (proc_desc_l4)
		== internal_freq_ok[num]),
	       "L4_ProcDescInternalFreq (kip, %i) == %i != %i",
	       num, L4_ProcDescInternalFreq (proc_desc_l4),
	       internal_freq_ok[num]);
	check ("[L4]", "L4_ProcDescExternalFreq",
	       (L4_ProcDescExternalFreq (proc_desc_l4)
		== external_freq_ok[num]),
	       "L4_ProcDescExternalFreq (kip, %i) == %i != %i",
	       num, L4_ProcDescExternalFreq (proc_desc_l4),
	       external_freq_ok[num]);
      }
#endif

      if (num > 0)
	{
	  _L4_word_t size = ((_L4_word_t) proc_desc)
	    - ((_L4_word_t) proc_desc_prev);
	  check_nr ("[intern]", "size of proc_desc",
		    size, proc_desc_size_ok);
	}
      proc_desc_prev = proc_desc;
    }
}


/* Test the page info field.  */
void
test_page_info (_L4_kip_t kip)
{
  word_t page_size_mask_ok = 0x00401000;
  word_t page_rights_ok = 0x6;

#ifdef _L4_INTERFACE_INTERN
  check_nr ("[intern]", "_L4_page_size_mask",
	    _L4_page_size_mask (kip), page_size_mask_ok);
  check_nr ("[intern]", "_L4_page_rights",
	    _L4_page_rights (kip), page_rights_ok);
#endif

#ifdef _L4_INTERFACE_GNU
  check_nr ("[GNU]", "l4_page_size_mask_from",
	 l4_page_size_mask_from (kip), page_size_mask_ok);
  check_nr ("[GNU]", "l4_page_size_mask",
	    l4_page_size_mask (), page_size_mask_ok);
  check_nr ("[GNU]", "l4_page_rights_from",
	    l4_page_rights_from (kip), page_rights_ok);
  check_nr ("[GNU]", "l4_page_rights", l4_page_rights (), page_rights_ok);
#endif

#ifdef _L4_INTERFACE_L4
  check_nr ("[L4]", "L4_PageSizeMask",
	    L4_PageSizeMask (kip), page_size_mask_ok);
  check_nr ("[L4]", "L4_PageRights",
	    L4_PageRights (kip), page_rights_ok);
#endif
}


/* Test the thread info field.  */
void
test_thread_info (_L4_kip_t kip)
{
  word_t thread_id_bits_ok = 0x11;
  word_t thread_id_system_base_ok = 0x10;
  word_t thread_id_user_base_ok = 0x30;

#ifdef _L4_INTERFACE_INTERN
  check_nr ("[intern]", "_L4_thread_id_bits",
	    _L4_thread_id_bits (kip), thread_id_bits_ok);
  check_nr ("[intern]", "_L4_thread_system_base",
	    _L4_thread_system_base (kip), thread_id_system_base_ok);
  check_nr ("[intern]", "_L4_thread_user_base",
	    _L4_thread_user_base (kip), thread_id_user_base_ok);
#endif

#ifdef _L4_INTERFACE_GNU
  check_nr ("[GNU]", "l4_thread_id_bits_from",
	    l4_thread_id_bits_from (kip), thread_id_bits_ok);
  check_nr ("[GNU]", "l4_thread_id_bits",
	    l4_thread_id_bits (), thread_id_bits_ok);
  check_nr ("[GNU]", "l4_thread_system_base_from",
	    l4_thread_system_base_from (kip), thread_id_system_base_ok);
  check_nr ("[GNU]", "l4_thread_system_base",
	    l4_thread_system_base (), thread_id_system_base_ok);
  check_nr ("[GNU]", "l4_thread_user_base_from",
	    l4_thread_user_base_from (kip), thread_id_user_base_ok);
  check_nr ("[GNU]", "l4_thread_user_base",
	    l4_thread_user_base (), thread_id_user_base_ok);
#endif

#ifdef _L4_INTERFACE_L4
  check_nr ("[intern]", "L4_ThreadIdBits",
	    L4_ThreadIdBits (kip), thread_id_bits_ok);
  check_nr ("[intern]", "L4_ThreadIdSystemBase",
	    L4_ThreadIdSystemBase (kip), thread_id_system_base_ok);
  check_nr ("[intern]", "L4_ThreadIdUserBase",
	    L4_ThreadIdUserBase (kip), thread_id_user_base_ok);
#endif
}


/* Test the clock info field.  */
void
test_clock_info (_L4_kip_t kip)
{
  word_t read_precision_ok = 0;
  word_t schedule_precision_ok = 0;

#ifdef _L4_INTERFACE_INTERN
  check_nr ("[intern]", "_L4_read_precision",
	    _L4_read_precision (kip), read_precision_ok);
  check_nr ("[intern]", "_L4_schedule_precision",
	    _L4_schedule_precision (kip), schedule_precision_ok);
#endif

#ifdef _L4_INTERFACE_GNU
  check_nr ("[GNU]", "l4_read_precision_from",
	    l4_read_precision_from (kip), read_precision_ok);
  check_nr ("[GNU]", "l4_read_precision",
	    l4_read_precision (), read_precision_ok);
  check_nr ("[GNU]", "l4_schedule_precision_from",
	    l4_schedule_precision_from (kip), schedule_precision_ok);
  check_nr ("[GNU]", "l4_schedule_precision",
	    l4_schedule_precision (), schedule_precision_ok);
#endif

#ifdef _L4_INTERFACE_L4
  check_nr ("[L4]", "L4_ReadPrecision",
	    L4_ReadPrecision (kip), read_precision_ok);
  check_nr ("[L4]", "l4_SchedulePrecision",
	    L4_SchedulePrecision (kip), schedule_precision_ok);
#endif
}


/* Test the utcb info field.  */
void
test_utcb_info (_L4_kip_t kip)
{
  word_t utcb_align_log2_ok = 0x09;
  word_t utcb_area_size_log2_ok = 0x0c;
  word_t utcb_size_ok = 0x200;

#ifdef _L4_INTERFACE_INTERN
  check_nr ("[intern]", "_L4_utcb_area_size_log2",
	    _L4_utcb_area_size_log2 (kip), utcb_area_size_log2_ok);
  check_nr ("[intern]", "_L4_utcb_alignment_log2",
	    _L4_utcb_alignment_log2 (kip), utcb_align_log2_ok);
  check_nr ("[intern]", "_L4_utcb_size",
	    _L4_utcb_size (kip), utcb_size_ok);
#endif

#ifdef _L4_INTERFACE_GNU
  check_nr ("[GNU]", "l4_utcb_area_size_log2_from",
	    l4_utcb_area_size_log2_from (kip), utcb_area_size_log2_ok);
  check_nr ("[GNU]", "l4_utcb_area_size_log2",
	    l4_utcb_area_size_log2 (), utcb_area_size_log2_ok);
  check_nr ("[GNU]", "l4_utcb_area_size",
	    l4_utcb_area_size (), 1 << utcb_area_size_log2_ok);
  check_nr ("[GNU]", "l4_utcb_alignment_log2_from",
	    l4_utcb_alignment_log2_from (kip), utcb_align_log2_ok);
  check_nr ("[GNU]", "l4_utcb_alignment_log2",
	    l4_utcb_alignment_log2 (), utcb_align_log2_ok);
  check_nr ("[GNU]", "l4_utcb_alignment_log2",
	    l4_utcb_alignment (), 1 << utcb_align_log2_ok);
  check_nr ("[GNU]", "l4_utcb_size_from",
	    l4_utcb_size_from (kip), utcb_size_ok);
  check_nr ("[GNU]", "l4_utcb_size",
	    l4_utcb_size (), utcb_size_ok);
#endif

#ifdef _L4_INTERFACE_L4
  check_nr ("[L4]", "L4_UtcbAreaSizeLog2",
	    L4_UtcbAreaSizeLog2 (kip), utcb_area_size_log2_ok);
  check_nr ("[L4]", "L4_UtcbAlignmentLog2",
	    L4_UtcbAlignmentLog2 (kip), utcb_align_log2_ok);
  check_nr ("[L4]", "L4_UtcbSize",
	    L4_UtcbSize (kip), utcb_size_ok);
#endif
}


/* Test the kip area info field.  */
void
test_kip_area_info (_L4_kip_t kip)
{
  word_t kip_area_size_log2_ok = 0x0c;

#ifdef _L4_INTERFACE_INTERN
  check_nr ("[intern]", "_L4_kip_area_size_log2",
	    _L4_kip_area_size_log2 (kip), kip_area_size_log2_ok);
#endif

#ifdef _L4_INTERFACE_GNU
  check_nr ("[GNU]", "l4_kip_area_size_log2_from",
	    l4_kip_area_size_log2_from (kip), kip_area_size_log2_ok);
  check_nr ("[GNU]", "l4_kip_area_size_log2",
	    l4_kip_area_size_log2 (), kip_area_size_log2_ok);
  check_nr ("[GNU]", "l4_kip_area_size",
	    l4_kip_area_size (), 1 << kip_area_size_log2_ok);
#endif

#ifdef _L4_INTERFACE_L4
  check_nr ("[L4]", "L4_KipAreaSizeLog2",
	    L4_KipAreaSizeLog2 (kip), kip_area_size_log2_ok);
#endif
}


/* Test the boot info field.  */
void
test_boot_info (_L4_kip_t kip)
{
  word_t boot_info_ok = 0x32600;

#ifdef _L4_INTERFACE_INTERN
  check_nr ("[intern]", "_L4_boot_info",
	    _L4_boot_info (kip), boot_info_ok);
#endif

#ifdef _L4_INTERFACE_GNU
  check_nr ("[GNU]", "l4_boot_info_from",
	    l4_boot_info_from (kip), boot_info_ok);
  check_nr ("[GNU]", "l4_boot_info",
	    l4_boot_info (), boot_info_ok);
#endif

#ifdef _L4_INTERFACE_L4
  check_nr ("[L4]", "L4_BootInfo",
	    L4_BootInfo (kip), boot_info_ok);
#endif
}


/* Test the system call links.  */
void
test_syscalls (_L4_kip_t kip)
{
#ifdef _L4_INTERFACE_INTERN
  word_t space_control_ok = 0x910;
  word_t thread_control_ok = 0x8e0;
  word_t processor_control_ok = 0x930;
  word_t memory_control_ok = 0x940;
  word_t ipc_ok = 0x800;
  word_t lipc_ok = 0x830;
  word_t unmap_ok = 0x8d0;
  word_t exchange_registers_ok = 0x860;
  word_t system_clock_ok = 0x870;
  word_t thread_switch_ok = 0x8b0;
  word_t schedule_ok = 0x8c0;
  word_t arch0_ok = 0x950;
  word_t arch1_ok = 0;
  word_t arch2_ok = 0;
  word_t arch3_ok = 0;

#define CHECK_ONE_SYSCALL_LINK(name)					\
  check_nr ("[intern]", #name " syscall link",				\
	    kip->name, name ## _ok);

  CHECK_ONE_SYSCALL_LINK (space_control);
  CHECK_ONE_SYSCALL_LINK (thread_control);
  CHECK_ONE_SYSCALL_LINK (processor_control);
  CHECK_ONE_SYSCALL_LINK (memory_control);
  CHECK_ONE_SYSCALL_LINK (ipc);
  CHECK_ONE_SYSCALL_LINK (lipc);
  CHECK_ONE_SYSCALL_LINK (unmap);
  CHECK_ONE_SYSCALL_LINK (exchange_registers);
  CHECK_ONE_SYSCALL_LINK (system_clock);
  CHECK_ONE_SYSCALL_LINK (thread_switch);
  CHECK_ONE_SYSCALL_LINK (schedule);
  CHECK_ONE_SYSCALL_LINK (arch0);
  CHECK_ONE_SYSCALL_LINK (arch1);
  CHECK_ONE_SYSCALL_LINK (arch2);
  CHECK_ONE_SYSCALL_LINK (arch3);
#undef CHECK_ONE_SYSCALL_LINK
#endif

}


void
test (void)
{
  _L4_kip_t kip;

  kip = test_magic ();
  test_api_and_kernel_id ();

  test_kernel_gen_date (kip);
  test_kernel_version (kip);
  test_kernel_supplier (kip);
  test_kernel_version_string (kip);
  test_kernel_feature (kip);

  test_processor_info (kip);
  test_page_info (kip);
  test_thread_info (kip);
  test_clock_info (kip);
  test_utcb_info (kip);
  test_kip_area_info (kip);
  test_boot_info (kip);

  test_syscalls (kip);
}
