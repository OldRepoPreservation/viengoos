/* Initialization code run first thing by the ELF startup code.  Hurd version.
   Copyright (C) 1995-1999,2000,01,02,03,04,2005 Free Software Foundation, Inc.
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

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sysdep.h>
#include <fpu_control.h>
#include <sys/param.h>
#include <sys/types.h>
#include <assert.h>
#include <libc-internal.h>

#include <ldsodefs.h>

/* The function is called from assembly stubs the compiler can't see.  */
static void init (int, char **, char **) __attribute__ ((used));

/* Set nonzero if we have to be prepared for more then one libc being
   used in the process.  Safe assumption if initializer never runs.  */
int __libc_multiple_libcs attribute_hidden = 1;

/* Remember the command line argument and enviroment contents for
   later calls of initializers for dynamic libraries.  */
int __libc_argc attribute_hidden;
char **__libc_argv attribute_hidden;

#ifndef SHARED
#include <hurd/startup.h>
extern struct hurd_startup_data *_hurd_startup_data;
#endif

static void
init (int argc, char **argv, char **envp)
{
#ifdef USE_NONOPTION_FLAGS
  extern void __getopt_clean_environment (char **);
#endif

  __libc_multiple_libcs = &_dl_starting_up && !_dl_starting_up;

  /* Make sure we don't initialize twice.  */
  if (!__libc_multiple_libcs)
    {
      /* Set the FPU control word to the proper default value if the
	 kernel would use a different value.  (In a static program we
	 don't have this information.)  */
#ifdef SHARED
      if (__fpu_control != GLRO(dl_fpu_control))
#endif
	__setfpucw (__fpu_control);
    }

  /* Save the command-line arguments.  */
  __libc_argc = argc;
  __libc_argv = argv;
  __environ = envp;

#ifndef SHARED
  /* First the initialization which normally would be done by the
     dynamic linker.  */
  _dl_non_dynamic_init ();
#endif

  __init_misc (argc, argv, envp);

#ifdef USE_NONOPTION_FLAGS
  /* This is a hack to make the special getopt in GNU libc working.  */
  __getopt_clean_environment (envp);
#endif

#ifdef SHARED
  __libc_global_ctors ();
#endif
}

#ifdef SHARED

strong_alias (init, _init);

extern void __libc_init_first (void);

void
__libc_init_first (void)
{
}

#else
extern void __libc_init_first (int argc, char **argv, char **envp);

void
__libc_init_first (int argc, char **argv, char **envp)
{
  init (argc, argv, envp);
}
#endif


/* This function is defined here so that if this file ever gets into
   ld.so we will get a link error.  Having this file silently included
   in ld.so causes disaster, because the _init definition above will
   cause ld.so to gain an init function, which is not a cool thing. */

extern void _dl_start (void) __attribute__ ((noreturn));

void
_dl_start (void)
{
  abort ();
}



#ifndef SHARED

/* We define the global data for libl4.  */
#include <l4/globals.h>
#include <l4/init.h>
#include <l4/stubs.h>
#include <l4/stubs-init.h>

#include <l4.h>

#include <hurd/types.h>
#include <hurd/startup.h>

/* FIXME! Use memory manager.  */

/* Allocate SIZE bytes according to FLAGS into container CONTAINER
   starting with index START.  *AMOUNT contains the number of bytes
   successfully allocated.  */
static l4_word_t
allocate (l4_thread_id_t server, hurd_cap_handle_t container,
	  l4_fpage_t start, l4_word_t size,
	  l4_word_t flags, l4_word_t *amount)
{
  l4_msg_t msg;
  l4_msg_tag_t tag;

  l4_msg_clear (msg);
  l4_set_msg_label (msg, 132 /* Magic number for container_allocate_id.  */);
  l4_msg_append_word (msg, container);
  l4_msg_append_word (msg, flags);
  l4_msg_append_word (msg, start);
  l4_msg_append_word (msg, size);

  l4_msg_load (msg);

  tag = l4_call (server);
  l4_msg_store (tag, msg);

  *amount = l4_msg_word (msg, 0);

  return l4_msg_label (msg);
}

/* Map the memory at offset OFFSET with size SIZE at address VADDR
   from the container CONT in the physical memory server PHYSMEM.  */
static l4_word_t
map (l4_thread_id_t server, hurd_cap_handle_t container,
     l4_word_t offset, size_t size,
     void *vaddr, l4_word_t rights)
{
  l4_msg_t msg;
  l4_msg_tag_t tag;

  /* Magic!  If the offset is above 0x8000000 then we need to allocate
     anonymous memory.  */
  if (offset >= 0x8000000)
    {
      l4_word_t amount;
      allocate (server, container, offset, size, 0, &amount);
    }

  l4_msg_clear (msg);
  l4_set_msg_label (msg, 134 /* XXX: Magic number for container_map_id.  */);
  l4_msg_append_word (msg, container);
  l4_msg_append_word (msg, offset | rights);
  l4_msg_append_word (msg, size);
  l4_msg_append_word (msg, (l4_word_t) vaddr);
  l4_msg_load (msg);

  tag = l4_call (server);
  l4_msg_store (tag, msg);

  return l4_msg_label (msg);
}



/* This is used all over the place.  */
struct hurd_startup_data *_hurd_startup_data;

extern ElfW(Phdr) *_dl_phdr;
extern size_t _dl_phnum;

/* All this stuff should actually be called at the beginning of
   sysdeps/generic/libc-start.c::__libc_start_main(), before TLS is
   set up.  */
static void
init1 (void)
{
  _dl_phdr = (ElfW(Phdr) *) _hurd_startup_data->phdr;
  _dl_phnum = _hurd_startup_data->phdr_len / sizeof (ElfW(Phdr));
  assert (_hurd_startup_data->phdr_len % sizeof (ElfW(Phdr)) == 0);

  __libc_init_secure ();
}


/* Initialize a statically linked application.  This is called by
   sysdeps/l4/hurd/i386/static-start.S.  The function returns the
   address of the new stack.  */
void *
_hurd_pre_start (struct hurd_startup_data *startup)
{
  l4_word_t stack_size;
  /* Top of stack.  */
  char *tos = (char *) 0xc0000000;
  struct hurd_startup_data *new_startup;
  int argc;

  /* Initialize the system call stubs.  */
  l4_init ();
  l4_init_stubs ();

  /* A small stack for now.  Somewhat arbitrary (4MB allocates one
     super-page).  */
  stack_size =  4 * 1024 * 1024;

  /* Let physmem take over the address space completely.  */
  l4_accept (l4_map_grant_items (L4_COMPLETE_ADDRESS_SPACE));

  /* Map in the top of the stack.  One page is enough for now
     (actually depends on argv and env - libhurd-mm should grow this
     automatically - FIXME).  */
  /* FIXME: Check PT_GNU_STACK if we want it to be fully accessible.  */
  map (startup->mapv[0].cont.server, startup->mapv[0].cont.cap_handle,
       tos - stack_size, stack_size, tos - stack_size,
       L4_FPAGE_FULLY_ACCESSIBLE);

  /* The startup data can easily be relocated.  */
  tos -= sizeof (struct hurd_startup_data);
  new_startup = (struct hurd_startup_data *) tos;

  /* Now relocate the startup data.  */
  *new_startup = *startup;

  tos -= startup->mapc * sizeof (struct hurd_startup_map);
  new_startup->mapv = (struct hurd_startup_map *) tos;
  memcpy (new_startup->mapv, startup->mapv,
	  startup->mapc * sizeof (struct hurd_startup_map));

  tos -= (startup->argz_len + sizeof (l4_word_t) - 1)
    & ~(sizeof (l4_word_t) - 1);
  new_startup->argz = tos;
  memcpy (new_startup->argz, startup->argz, startup->argz_len);

  tos -= (startup->envz_len + sizeof (l4_word_t) - 1)
    & ~(sizeof (l4_word_t) - 1);
  new_startup->envz = tos;
  memcpy (new_startup->envz, startup->envz, startup->envz_len);

  /* Now push the ELF data on the stack as specified by System V ABI,
     ia32 Supplement, p 3-28, in reverse order.  */

  /* First the NULL-terminated aux vector.  */
  tos -= sizeof (void *);
  *((void **) tos) = 0;
  /* FIXME: Define an extension.  */
#define AT_GNU_HURD_STARTUP_DATA	16384
  tos -= sizeof (Elf32_auxv_t);
  *((Elf32_auxv_t *) tos) = (Elf32_auxv_t) { .a_type=AT_GNU_HURD_STARTUP_DATA,
					     .a_un.a_ptr=new_startup };

#define push_argz(argz,argz_len)					\
({									\
  int count = 0;							\
  char *start = argz;							\
  char *end = start + argz_len;						\
									\
  tos -= sizeof (void *);						\
  *((void **) tos) = 0;							\
									\
  while (end > start)							\
    {									\
      /* Look for the beginning of the last string.  */			\
      char *str = memrchr (start, 0, end - start - 1);			\
									\
      if (!str)								\
        str = start;							\
      else								\
        str++;								\
									\
      tos -= sizeof (char *);						\
      *((char **) tos) = str;						\
									\
      count++;								\
      end = str;							\
    }									\
  count;								\
})
  
  /* Now the env and arg vector.  */
  push_argz (new_startup->envz, new_startup->envz_len);
  argc = push_argz (new_startup->argz, new_startup->argz_len);

  tos -= sizeof (int);
  *((int **) tos) = argc;

  /* We are done.  Our stack now looks exactly as required by the
     System V ABI.  We can now return its address to the caller, who
     will fire up glibc for real.  */

  _hurd_startup_data = new_startup;

  /* But before we return, we do some hack magic.  Putting it here
     avoids modifying "generic" glibc files.  */
  init1 ();

  return tos;
}

#endif	/* ! SHARED */
