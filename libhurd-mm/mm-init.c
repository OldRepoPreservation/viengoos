/* mm-init.h - Memory management initialization.
   Copyright (C) 2004, 2005, 2008 Free Software Foundation, Inc.
   Written by Neal H. Walfield <neal@gnu.org>.

   This file is part of the GNU Hurd.

   The GNU Hurd is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2, or (at
   your option) any later version.
   
   The GNU Hurd is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.
   
   You should have received a copy of the GNU General Public License
   along with the GNU Hurd; see the file COPYING.  If not, write to
   the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139,
   USA.  */

#if HAVE_CONFIG_H
#include <config.h>
#endif

#include <hurd/startup.h>
#include <hurd/exceptions.h>
#include <hurd/thread.h>

#ifdef i386
#include <hurd/pager.h>
#endif

#include <backtrace.h>

#include "storage.h"
#include "as.h"

extern struct hurd_startup_data *__hurd_startup_data;

addr_t meta_data_activity;

int mm_init_done;

void
mm_init (addr_t activity)
{
  assert (! mm_init_done);

  extern int output_debug;
  output_debug = 1;

  if (ADDR_IS_VOID (activity))
    meta_data_activity = __hurd_startup_data->activity;
  else
    meta_data_activity = activity;

  hurd_activation_handler_init_early ();
  storage_init ();
  as_init ();
  hurd_activation_handler_init ();

  mm_init_done = 1;

#ifndef NDEBUG
  /* The following test checks the activation trampoline.  In
     particular, it checks that the register file before a fault
     matches the register file after a fault.  This is interesting
     because such an activation is handled in normal mode.  That
     means, when the fault occurs, we enter the activation handler,
     return an activation frame, enter normal mode, execute the normal
     mode activation handler, call the call back functions, and then
     return to the interrupted code.  */
#ifdef i386
  void test (int nesting)
  {
    addr_t addr = as_alloc (PAGESIZE_LOG2, 1, true);
    void *a = ADDR_TO_PTR (addr_extend (addr, 0, PAGESIZE_LOG2));

    int recursed = false;

    struct storage storage;
    bool fault (struct pager *pager,
		uintptr_t offset, int count, bool ro,
		uintptr_t fault_addr, uintptr_t ip,
		struct activation_fault_info info)
    {
      assert (a == (void *) (fault_addr & ~(PAGESIZE - 1)));
      assert (count == 1);

      struct hurd_utcb *utcb = hurd_utcb ();
      struct activation_frame *activation_frame = utcb->activation_stack;
      debug (4, "Fault at %p (ip: %p, sp: %p, eax: %p, "
	     "ebx: %p, ecx: %p, edx: %p, edi: %p, esi: %p, ebp: %p, "
	     "eflags: %p)",
	     fault,
	     (void *) activation_frame->eip,
	     (void *) activation_frame->esp,
	     (void *) activation_frame->eax,
	     (void *) activation_frame->ebx,
	     (void *) activation_frame->ecx,
	     (void *) activation_frame->edx,
	     (void *) activation_frame->edi,
	     (void *) activation_frame->esi,
	     (void *) activation_frame->ebp,
	     (void *) activation_frame->eflags);

      assert (activation_frame->eax == 0xa);
      assert (activation_frame->ebx == 0xb);
      assert (activation_frame->ecx == 0xc);
      assert (activation_frame->edx == 0xd);
      assert (activation_frame->edi == 0xd1);
      assert (activation_frame->esi == 0x21);
      assert (activation_frame->ebp == (uintptr_t) a);
      /* We cannot easily check esp and eip here.  */

      as_ensure (addr);
      storage = storage_alloc (ADDR_VOID,
			       cap_page, STORAGE_UNKNOWN,
			       OBJECT_POLICY_DEFAULT,
			       addr);

      if (nesting > 1 && ! recursed)
	{
	  recursed = true;

	  int i;
	  for (i = 0; i < 3; i ++)
	    {
	      debug (5, "Depth: %d; iter: %d", nesting - 1, i);
	      test (nesting - 1);
	      debug (5, "Depth: %d; iter: %d done", nesting - 1, i);
	    }
	}

      return true;
    }

    struct pager pager = PAGER_VOID;
    pager.length = PAGESIZE;
    pager.fault = fault;
    pager_init (&pager);

    struct region region = { (uintptr_t) a, PAGESIZE };
    struct map *map = map_create (region, MAP_ACCESS_ALL, &pager, 0, NULL);

    uintptr_t pre_flags, pre_esp;
    uintptr_t eax, ebx, ecx, edx, edi, esi, ebp, esp, flags;
    uintptr_t canary;

    /* Check that the trampoline works.  */
    __asm__ __volatile__
      (
       "mov %%esp, %[pre_esp]\n\t"
       "pushf\n\t"
       "pop %%eax\n\t"
       "mov %%eax, %[pre_flags]\n\t"

       /* Canary.  */
       "pushl $0xcab00d1e\n\t"

       "pushl %%ebp\n\t"

       "mov $0xa, %%eax\n\t"
       "mov $0xb, %%ebx\n\t"
       "mov $0xc, %%ecx\n\t"
       "mov $0xd, %%edx\n\t"
       "mov $0xd1, %%edi\n\t"
       "mov $0x21, %%esi\n\t"
       "mov %[addr], %%ebp\n\t"
       /* Fault!  */
       "mov %%eax, 0(%%ebp)\n\t"

       /* Save the current ebp.  */
       "pushl %%ebp\n\t"
       /* Restore the old ebp.  */
       "mov 4(%%esp), %%ebp\n\t"

       /* Save the rest of the GP registers.  */
       "mov %%eax, %[eax]\n\t"
       "mov %%ebx, %[ebx]\n\t"
       "mov %%ecx, %[ecx]\n\t"
       "mov %%edx, %[edx]\n\t"
       "mov %%edi, %[edi]\n\t"
       "mov %%esi, %[esi]\n\t"

       /* Save the new flags.  */
       "pushf\n\t"
       "pop %%eax\n\t"
       "mov %%eax, %[flags]\n\t"

       /* Save the new ebp.  */
       "mov 0(%%esp), %%eax\n\t"
       "mov %%eax, %[ebp]\n\t"

       /* Fix up the stack.  */
       "add $8, %%esp\n\t"

       /* Grab the canary.  */
       "popl %%eax\n\t"
       "mov %%eax, %[canary]\n\t"

       /* And don't forget to save the new esp.  */
       "mov %%esp, %[esp]\n\t"

       : [eax] "=m" (eax), [ebx] "=m" (ebx),
	 [ecx] "=m" (ecx), [edx] "=m" (edx),
	 [edi] "=m" (edi), [esi] "=m" (esi), [ebp] "=m" (ebp),
	 [pre_esp] "=m" (pre_esp), [esp] "=m" (esp),
	 [pre_flags] "=m" (pre_flags), [flags] "=m" (flags),
	 [canary] "=m" (canary)
       : [addr] "m" (a)
       : "%eax", "%ebx", "%ecx", "%edx", "%edi", "%esi");

    debug (4, "Regsiter file: "
	   "eax: %p, ebx: %p, ecx: %p, edx: %p, "
	   "edi: %p, esi: %p, ebp: %p -> %p, esp: %p -> %p, flags: %p -> %p",
	   (void *) eax, (void *)  ebx, (void *)  ecx, (void *)  edx,
	   (void *) edi, (void *)  esi, (void *) a, (void *)  ebp,
	   (void *) pre_esp, (void *) esp,
	   (void *) pre_flags, (void *) flags);

    assert (eax == 0xa);
    assert (ebx == 0xb);
    assert (ecx == 0xc);
    assert (edx == 0xd);
    assert (edi == 0xd1);
    assert (esi == 0x21);
    assert (ebp == (uintptr_t) a);
    assert (esp == pre_esp);
    assert (flags == pre_flags);
    assert (canary == 0xcab00d1e);

    maps_lock_lock ();
    map_disconnect (map);
    maps_lock_unlock ();
    map_destroy (map);

    storage_free (storage.addr, false);
    as_free (addr, 1);
  }

  int i;
  for (i = 0; i < 3; i ++)
    {
      debug (5, "Depth: %d; iter: %d", 3, i + 1);
      test (3);
      debug (5, "Depth: %d; iter: %d done", 3, i + 1);
    }
#endif
#endif
}
