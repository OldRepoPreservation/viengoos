/* t-vregs.c - A test for the interfaces to the L4 message registers.
   Copyright (C) 2005 Free Software Foundation, Inc.
   Written by Matthieu Lemerre <racin@free.fr> and
   Marcus Brinkmann <marcus@gnu.org>.

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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>
#include <stddef.h>
#include <stdio.h>

/* This must be included before any libl4 header.  */
#include "environment.h"


#include <l4/message.h>


/* Test the message register MR with value VAL_OK.  */
void
test_one_mr (int mr, _L4_word_t val_ok)
{
  {
    _L4_word_t val;
    char *msg;

    /* Clear all MRs.  */
    memset (environment_utcb, ~(val_ok & 0xff), sizeof (environment_utcb));

    asprintf (&msg, "_L4_load_mr and _L4_store_mr with MR%02i and value 0x%x",
	      mr, val_ok);

    _L4_load_mr (mr, val_ok);
    _L4_store_mr (mr, &val);
    check_nr ("[intern]", msg, val, val_ok);
  }

#ifdef _L4_INTERFACE_GNU
  {
    l4_word_t val;
    char *msg;

    /* Clear all MRs.  */
    memset (environment_utcb, ~(val_ok & 0xff), sizeof (environment_utcb));

    asprintf (&msg, "l4_load_mr and l4_store_mr with MR%02i and value 0x%x",
	      mr, val_ok);

    l4_load_mr (mr, val_ok);
    l4_store_mr (mr, &val);
    check_nr ("[GNU]", msg, val, val_ok);
  }
#endif

#ifdef _L4_INTERFACE_L4
  {
    L4_Word_t val;
    char *msg;

    /* Clear all MRs.  */
    memset (environment_utcb, ~(val_ok & 0xff), sizeof (environment_utcb));

    asprintf (&msg, "L4_LoadMR and L4_StoreMR with MR%02i and value 0x%x",
	      mr, val_ok);

    L4_LoadMR (mr, val_ok);
    L4_StoreMR (mr, &val);
    check_nr ("[L4]", msg, val, val_ok);
  }
#endif
}


/* Test the message registers START_MR to START_MR + NUM - 1
   with the values VALUES_OK.  */
void
test_many_mrs (int start_mr, int nr, _L4_word_t *values_ok)
{
  {
    _L4_word_t values[nr];
    int i;

    /* Clear all MRs with a pattern that is unlike the test pattern.  */
    memset (environment_utcb,
	    ~((values_ok[0] & 0x0f) | (values_ok[nr - 1] & 0xf0)),
	    sizeof (environment_utcb));

    _L4_load_mrs (start_mr, nr, values_ok);
    _L4_store_mrs (start_mr, nr, values);

    for (i = 0; i < nr; i++)
      {
	char *msg;
	asprintf (&msg,
		  "_L4_load_mrs, _L4_store_mrs MR%02i-%02i (MR%02i, 0x%x)",
		  start_mr, start_mr + nr - 1, start_mr + i, values_ok[i]);
	
	check_nr ("[intern]", msg, values[i], values_ok[i]);
      }
  }

#ifdef _L4_INTERFACE_GNU
  {
    l4_word_t values[nr];
    int i;

    /* Clear all MRs with a pattern that is unlike the test pattern.  */
    memset (environment_utcb,
	    ~((values_ok[0] & 0x0f) | (values_ok[nr - 1] & 0xf0)),
	    sizeof (environment_utcb));

    l4_load_mrs (start_mr, nr, values_ok);
    l4_store_mrs (start_mr, nr, values);

    for (i = 0; i < nr; i++)
      {
	char *msg;
	asprintf (&msg,
		  "l4_load_mrs, l4_store_mrs MR%02i-%02i (MR%02i, 0x%x)",
		  start_mr, start_mr + nr - 1, start_mr + i, values_ok[i]);
	
	check_nr ("[GNU]", msg, values[i], values_ok[i]);
      }
  }
#endif

#ifdef _L4_INTERFACE_L4
  {
    L4_Word_t values[nr];
    int i;

    /* Clear all MRs with a pattern that is unlike the test pattern.  */
    memset (environment_utcb,
	    ~((values_ok[0] & 0x0f) | (values_ok[nr - 1] & 0xf0)),
	    sizeof (environment_utcb));

    L4_LoadMRs (start_mr, nr, values_ok);
    L4_StoreMRs (start_mr, nr, values);

    for (i = 0; i < nr; i++)
      {
	char *msg;
	asprintf (&msg,
		  "L4_LoadMRs, L4_StoreMRs MR%02i-%02i (MR%02i, 0x%x)",
		  start_mr, start_mr + nr - 1, start_mr + i, values_ok[i]);
	
	check_nr ("[L4]", msg, values[i], values_ok[i]);
      }
  }
#endif
}


void
test_mrs (void)
{
  static struct
  {
    int start_mr;
    int nr;
    _L4_word_t values[L4_NUM_MRS];
  } tests[] =
    {
      {  0, 1, { 22 } },
      { 63, 1, { 23 } },
      {  5, 3, { 1, 2, 3 } },
      {  0, 64, { -1, 2, -3, 4, -5, 6, -7, 8,
		  9, -10, 11, -12, 13, -14, 15, -16,
		  -17, 18, -19, 20, -21, 22, -23, 24,
		  25, -26, 27, -28, 29, -30, 31, -32,
		  -33, 34, -35, 36, -37, 38, -39, 40,
		  41, -42, 43, -44, 45, -46, 47, -48,
		  -49, 50, -51, 52, -53, 54, -55, 56,
		  57, -58, 59, -60, 61, -62, 63, -64 } }
    };
  int nr = sizeof (tests) / sizeof (tests[0]);
  int i;

  check_nr ("[intern]", "_L4_NUM_MRS", _L4_NUM_MRS, 64);
#ifdef _L4_INTERFACE_GNU
  check_nr ("[GNU]", "L4_NUM_MRS", L4_NUM_MRS, 64);
#endif
  /* FIXME: There is no L4 interface for this.  */

  for (i = 0; i < nr; i++)
    if (tests[i].nr == 1)
      test_one_mr (tests[i].start_mr, tests[i].values[0]);
    else
      test_many_mrs (tests[i].start_mr, tests[i].nr, &tests[i].values[0]);
}


/* Test the message register BR with value VAL_OK.  */
void
test_one_br (int br, _L4_word_t val_ok)
{
  {
    _L4_word_t val;
    char *msg;

    /* Clear all BRs.  */
    memset (environment_utcb, ~(val_ok & 0xff), sizeof (environment_utcb));

    asprintf (&msg, "_L4_load_br and _L4_store_br with BR%02i and value 0x%x",
	      br, val_ok);

    _L4_load_br (br, val_ok);
    _L4_store_br (br, &val);
    check_nr ("[intern]", msg, val, val_ok);
  }

#ifdef _L4_INTERFACE_GNU
  {
    l4_word_t val;
    char *msg;

    /* Clear all BRs.  */
    memset (environment_utcb, ~(val_ok & 0xff), sizeof (environment_utcb));

    asprintf (&msg, "l4_load_br and l4_store_br with BR%02i and value 0x%x",
	      br, val_ok);

    l4_load_br (br, val_ok);
    l4_store_br (br, &val);
    check_nr ("[GNU]", msg, val, val_ok);
  }
#endif

#ifdef _L4_INTERFACE_L4
  {
    L4_Word_t val;
    char *msg;

    /* Clear all BRs.  */
    memset (environment_utcb, ~(val_ok & 0xff), sizeof (environment_utcb));

    asprintf (&msg, "L4_LoadBR and L4_StoreBR with BR%02i and value 0x%x",
	      br, val_ok);

    L4_LoadBR (br, val_ok);
    L4_StoreBR (br, &val);
    check_nr ("[L4]", msg, val, val_ok);
  }
#endif
}


/* Test the message registers START_BR to START_BR + NUM - 1
   with the values VALUES_OK.  */
void
test_many_brs (int start_br, int nr, _L4_word_t *values_ok)
{
  {
    _L4_word_t values[nr];
    int i;

    /* Clear all BRs with a pattern that is unlike the test pattern.  */
    memset (environment_utcb,
	    ~((values_ok[0] & 0x0f) | (values_ok[nr - 1] & 0xf0)),
	    sizeof (environment_utcb));

    _L4_load_brs (start_br, nr, values_ok);
    _L4_store_brs (start_br, nr, values);

    for (i = 0; i < nr; i++)
      {
	char *msg;
	asprintf (&msg,
		  "_L4_load_brs, _L4_store_brs BR%02i-%02i (BR%02i, 0x%x)",
		  start_br, start_br + nr - 1, start_br + i, values_ok[i]);
	
	check_nr ("[intern]", msg, values[i], values_ok[i]);
      }
  }

#ifdef _L4_INTERFACE_GNU
  {
    l4_word_t values[nr];
    int i;

    /* Clear all BRs with a pattern that is unlike the test pattern.  */
    memset (environment_utcb,
	    ~((values_ok[0] & 0x0f) | (values_ok[nr - 1] & 0xf0)),
	    sizeof (environment_utcb));

    l4_load_brs (start_br, nr, values_ok);
    l4_store_brs (start_br, nr, values);

    for (i = 0; i < nr; i++)
      {
	char *msg;
	asprintf (&msg,
		  "l4_load_brs, l4_store_brs BR%02i-%02i (BR%02i, 0x%x)",
		  start_br, start_br + nr - 1, start_br + i, values_ok[i]);
	
	check_nr ("[GNU]", msg, values[i], values_ok[i]);
      }
  }
#endif

#ifdef _L4_INTERFACE_L4
  {
    L4_Word_t values[nr];
    int i;

    /* Clear all BRs with a pattern that is unlike the test pattern.  */
    memset (environment_utcb,
	    ~((values_ok[0] & 0x0f) | (values_ok[nr - 1] & 0xf0)),
	    sizeof (environment_utcb));

    L4_LoadBRs (start_br, nr, values_ok);
    L4_StoreBRs (start_br, nr, values);

    for (i = 0; i < nr; i++)
      {
	char *msg;
	asprintf (&msg,
		  "L4_LoadBRs, L4_StoreBRs BR%02i-%02i (BR%02i, 0x%x)",
		  start_br, start_br + nr - 1, start_br + i, values_ok[i]);
	
	check_nr ("[L4]", msg, values[i], values_ok[i]);
      }
  }
#endif
}


void
test_brs (void)
{
  static struct
  {
    int start_br;
    int nr;
    _L4_word_t values[L4_NUM_BRS];
  } tests[] =
    {
      {  0, 1, { 24 } },
      { 63, 1, { 25 } },
      {  5, 3, { 4, 5, 6 } },
      {  0, 34, { -1, 2, -3, 4, -5, 6, -7, 8,
		  9, -10, 11, -12, 13, -14, 15, -16,
		  -17, 18, -19, 20, -21, 22, -23, 24,
		  25, -26, 27, -28, 29, -30, 31, -32,
		  -33 } }
    };
  int nr = sizeof (tests) / sizeof (tests[0]);
  int i;

  check_nr ("[intern]", "_L4_NUM_BRS", _L4_NUM_BRS, 33);
#ifdef _L4_INTERFACE_GNU
  check_nr ("[GNU]", "L4_NUM_BRS", L4_NUM_BRS, 33);
#endif
  /* FIXME: There is no L4 interface for this.  */

  for (i = 0; i < nr; i++)
    if (tests[i].nr == 1)
      test_one_br (tests[i].start_br, tests[i].values[0]);
    else
      test_many_brs (tests[i].start_br, tests[i].nr, &tests[i].values[0]);
}


void
test (void)
{
  test_mrs ();
  test_brs ();
}
