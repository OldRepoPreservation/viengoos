/* t-environment.h - Setup test environment.
 */

/* This file is divided into two sections: common declarations and
   start up code.

   The former section creates the testing environment that influences
   every file.  By including this file using the CPP -include
   argument, most files shouldn't have to be changed to be made aware
   of the testing environment; they will just compile.  For
   functionality that does need to be changed, this file also defines
   the _L4_TEST_ENVIRONMENT macro.  Code can test this to decide which
   implementation to use.

   The latter section consists of start up code and convenience code.
   Whatever file is the mail file should first define the
   _L4_TEST_MAIN macro and then include this file.  The program then
   need only define a test function.  */

#ifndef T_ENVIRONMENT_H
#define T_ENVIRONMENT_H

#include <hurd/stddef.h>
#include <stdio.h>

/* FIXME: We can not include stdlib.h, as this wants to suck in the
   whole libl4 headers via pthread (which currently fails as pthread
   doesn't include the header files).  Ouch!  */
extern char *getenv (const char *name);
extern void exit (int status) __attribute__ ((__noreturn__));


/* A type that behaves like char * alias-wise, but has the width of
   the system word size.  */
#ifdef __i386__
typedef unsigned int __attribute__((__mode__ (__SI__))) word_t;
typedef word_t __attribute__((__may_alias__)) word_ma_t;
#else
#error not ported to this architecture
#endif

/* Our kernel interface page.  */
#ifdef __i386__
static const word_ma_t environment_kip[] = 
  {
    /* 0x0000 */ 0x4be6344c, 0x84050000, 0x00000000, 0x00000140,
    /* 0x0010 */ 0x0014fab0, 0xf0129720, 0x00000000, 0x00000000,
    /* 0x0020 */ 0x00000000, 0x00041c70, 0x00040000, 0x000483a0,
    /* 0x0030 */ 0x00000000, 0x00000000, 0x00000000, 0x00000000,
    /* 0x0040 */ 0x00000000, 0x00300000, 0x00300000, 0x0030ba90,
    /* 0x0050 */ 0x00000000, 0x01d00007, 0x00000000, 0x00000000,
    /* 0x0060 */ 0x00000000, 0x00000000, 0x00100200, 0x0014f000,
    /* 0x0070 */ 0x00000000, 0x00000000, 0x00000000, 0x00000000,
    /* 0x0080 */ 0x00000000, 0x00000000, 0x00000000, 0x00000000,
    /* 0x0090 */ 0x00000000, 0x00000000, 0x00000000, 0x00000000,
    /* 0x00a0 */ 0x00000000, 0x00000000, 0x000c2401, 0x0000000c,
    /* 0x00b0 */ 0x00000000, 0x00000000, 0x00032600, 0x00000120,
    /* 0x00c0 */ 0x00000000, 0x03001011, 0x00401006, 0x40000001,
    /* 0x00d0 */ 0x00000910, 0x000008e0, 0x00000930, 0x00000940,
    /* 0x00e0 */ 0x00000800, 0x00000830, 0x000008d0, 0x00000860,
    /* 0x00f0 */ 0x00000870, 0x000008b0, 0x000008c0, 0x00000000,

    /* 0x0100 */ 0x00000000, 0x00000000, 0x00000000, 0x00000000,
    /* 0x0110 */ 0x00000950, 0x00000000, 0x00000000, 0x00000000,
    /* 0x0120 */ 0x00000000, 0x002addde, 0x00000000, 0x00000000,
    /* 0x0130 */ 0x00000000, 0x002adddf, 0x00000000, 0x00000000,
    /* 0x0140 */ 0x04020000, 0x00000a36, 0x00040000, 0x20614b55,
    /* 0x0150 */ 0x614b344c, 0x69503a3a, 0x63617473, 0x206f6968,
    /* 0x0160 */ 0x7562202d, 0x20746c69, 0x4a206e6f, 0x32206e61,
    /* 0x0170 */ 0x30322032, 0x30203530, 0x36323a32, 0x2034313a,
    /* 0x0180 */ 0x6d207962, 0x75637261, 0x6c754073, 0x65737379,
    /* 0x0190 */ 0x73752073, 0x20676e69, 0x20636367, 0x73726576,
    /* 0x01a0 */ 0x206e6f69, 0x2e332e33, 0x44282034, 0x61696265,
    /* 0x01b0 */ 0x3a31206e, 0x2e332e33, 0x33312d34, 0x6d730029,
    /* 0x01c0 */ 0x736c6c61, 0x65636170, 0x00000073, 0x00000000,
    /* 0x01d0 */ 0x00000004, 0xfffffc00, 0x00000001, 0x0009f800,
    /* 0x01e0 */ 0x00100001, 0x07fffc00, 0x000a0004, 0x000efc00,
    /* 0x01f0 */ 0x07000002, 0x08000000, 0x00000201, 0xbffffc00,
    /* 0x0200 */ 0x00100002, 0x0014ec00, 0x00000000, 0x00000000

    /* The rest in the real KIP are 0x00, until offset 0x800, which
       contains the system call stubs.  */
  };

static word_t __attribute__((__unused__)) environment_api_version = 0x84050000;
static word_t __attribute__((__unused__)) environment_api_flags = 0x00000000;
static word_t __attribute__((__unused__)) environment_kernel_id = 0x04020000;

/* 64 MRs forwards, 16 UTCB words and 33 BRs backwards.  */
static word_t environment_utcb[64 + 16 + 33];
static word_t *environment_utcb_address = &environment_utcb[33 + 16];
#else
#error not ported to this architecture
#endif


#ifdef _L4_TEST_EXTERNAL_LIBL4

#warning Add support for your external libl4 here.

/* Only check the official interface.  */
#undef _L4_INTERFACE_INTERN
#undef _L4_INTERFACE_GNU
#define _L4_INTERFACE_L4	1

#else	/* _L4_TEST_EXTERNAL_LIBL4 */

/* This signals to libl4 that we are running in a fake test
   environment.  */
#define _L4_TEST_ENVIRONMENT	1

/* Our implementation of the kernel interface system call.  */
#define _L4_TEST_KERNEL_INTERFACE_IMPL		\
  *api_version = environment_api_version;	\
  *api_flags = environment_api_flags;		\
  *kernel_id = environment_kernel_id;		\
  return (_L4_kip_t) environment_kip;

/* Our implementation of the "get utcb address" function.  */
#define _L4_TEST_UTCB_IMPL \
  return (_L4_word_t *) environment_utcb_address;

/* Enable all interfaces.  */
#define _L4_INTERFACE_INTERN	1
#define _L4_INTERFACE_L4	1
#define _L4_INTERFACE_GNU	1

#include <l4/features.h>

/* If you want to test if you wrote the tests for the various
   interfaces independently of each other, enable one of these.  */
#if 0
/* Only the official interface.  */
#undef _L4_INTERFACE_INTERN
#undef _L4_INTERFACE_GNU
#elsif 0
/* Only the GNU interface.  */
#undef _L4_INTERFACE_INTERN
#undef _L4_INTERFACE_L4
#elsif 0
/* Only the internal interface.  */
#undef _L4_INTERFACE_GNU
#undef _L4_INTERFACE_L4
#endif

#endif	/* _L4_TEST_EXTERNAL_LIBL4 */


#ifdef _L4_INTERFACE_GNU

/* Include the global variables that need to be available in every
   program.  They are initialized by INIT.  */
#include <l4/globals.h>

#endif	/* _L4_INTERFACE_GNU */
#endif  /* T_ENVIRONMENT_H */

#ifdef _L4_TEST_MAIN
#include <string.h>
#include <l4/stubs.h>

const char program_name[100];

/* Be verbose.  */
static int opt_verbose;

/* Do not exit if errors occur.  */
static int opt_keep_going;


/* True if a check failed.  */
static int failed;


/* Initialize the fake environment.  */
void
static inline environment_init (int argc, char *argv[])
{
  strncpy ((char *) program_name, argv[0], sizeof (program_name) - 1);

  int i;

#if _L4_INTERFACE_GNU
  __l4_kip = (_L4_kip_t) environment_kip;
#endif

  for (i = 0; i < argc; i++)
    {
      char *arg;

      if (i == 0)
	{
	  arg = getenv ("TESTOPTS");
	  if (!arg)
	    continue;
	}
      else
	{
	  arg = argv[i];

	  if (arg[0] != '-')
	    continue;
	  arg++;
	}

      while (*arg)
	  {
	    switch (*arg)
	      {
	      case 'v':
		opt_verbose = 1;
		break;

	      case 'k':
		opt_keep_going = 1;
		break;

	      default:
		fprintf (stderr, "%s: warning: ignoring unknown option -%c\n",
			 argv[0], *arg);
		break;
	      }
	    arg++;
	  }
    }
}


/* Support macros.  */

#include <stdio.h>

#define check(prefix,msg,cond,...) \
  do								\
    {								\
      if (opt_verbose)						\
        printf ("%s Checking %s... ", prefix, msg);		\
      if (cond)							\
        {							\
          if (opt_verbose)					\
            printf ("OK\n");					\
        }							\
      else							\
        {							\
          if (opt_verbose)					\
            printf ("failed\n");				\
          fprintf (stderr, "FAIL: %s ", prefix);		\
          fprintf (stderr, __VA_ARGS__);			\
          fprintf (stderr, "\n");				\
          failed = 1;						\
          if (!opt_keep_going)					\
	    exit (1);						\
        }							\
    }								\
  while (0)

#define check_nr(prefix,msg,val1,val2) \
  do								\
    {								\
      typeof(val1) v1 = (val1);					\
      typeof(val2) v2 = (val2);					\
								\
      check (prefix, msg, (v1 == v2), #val1 " == 0x%x != 0x%x",	\
	     v1, v2);						\
    }								\
  while (0)


void test (void);


int
main (int argc, char *argv[])
{
  /* Initialize the test environment.  */
  environment_init (argc, argv);

  output_init ("stdio");

  test ();

  return failed ? 1 : 0;
}
#endif  /* _L4_TEST_MAIN */
