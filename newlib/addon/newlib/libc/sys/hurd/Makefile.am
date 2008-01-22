## Process this file with automake to generate Makefile.in

AUTOMAKE_OPTIONS = cygnus

INCLUDES = -I$(srcdir)/include $(NEWLIB_CFLAGS) $(CROSS_CFLAGS) $(TARGET_CFLAGS) $(INCLTDL)

if ELIX_LEVEL_1
argp_dir=
ARGP_LIB=
else
argp_dir=
#../linux/argp
ARGP_LIB=
#../linux/argp/libargp.la
endif

# ../linux/cmath
SUBDIRS = $(argp_dir) \
	$(EXTRA_SUBDIRS)\
	.
# cmath/libcmath.la
SUBLIBS = \
	$(ARGP_LIB) \
	$(EXTRA_SUBLIBS) \
	$(HURD_MACH_LIB)

# Machine dependent source files.  Provided in $(machine_dir).
MACHINE_SOURCES = 

GENERAL_SOURCES = \
	closer.c \
	reent.c \
	fcntlr.c \
	fstatr.c \
	gettimeofdayr.c \
	linkr.c \
	lseekr.c \
	openr.c \
	readr.c \
	renamer.c \
	signalr.c \
	sbrkr.c \
	statr.c \
	timesr.c \
	unlinkr.c \
	writer.c \
	getreent.c \
	malloc.c \
	mallocr.c \
	crt0.c \
	$(addprefix $(machine_dir)/,$(MACHINE_SOURCES))

if USE_LIBTOOL
noinst_LTLIBRARIES = libhurd.la
libhurd_la_SOURCES = $(GENERAL_SOURCES) $(ELIX_SOURCES)
libhurd_la_LIBADD = $(ADD_OBJS)
libhurd_la_DEPENDENCIES = $(ADD_OBJS)
LIBC_COMPILE = $(LTCOMPILE)
noinst_DATA = objectlist.awk.in
else
noinst_LIBRARIES = lib.a
lib_a_SOURCES = $(GENERAL_SOURCES) $(ELIX_SOURCES)
lib_a_LIBADD = $(ADD_OBJS) $(SUBLIBS)
lib_a_DEPENDENCIES = $(ADD_OBJS)
lib_a_CFLAGS = $(AM_CFLAGS)
LIBC_COMPILE = $(COMPILE)
noinst_DATA =

endif # USE_LIBTOOL

objectlist.awk.in: $(noinst_LTLIBRARIES) $(SUBLIBS)
	-rm -f objectlist.awk.in
	for i in `ls *.lo` ; \
	  do \
	    echo $$i `pwd`/$$i >> objectlist.awk.in ; \
	done
	for j in $(SUBLIBS) ; \
	  do \
	    export i=`echo $$j | sed -e 's,\(.*\)\/[^\/]*$$,\1,'`; \
	    if test $$i != "." && test -f $$i/objectlist.awk.in; then \
	    	cat $$i/objectlist.awk.in >> objectlist.awk.in ; \
	    fi; \
	done

# The parent directory wants a file called crt0.o
all: crt0.o
crt0.o: lib_a-crt0.o
	cp $< $@

AM_CFLAGS = -I $(srcdir)/../../stdio
ACLOCAL_AMFLAGS = -I ../../.. -I ../../../..
CONFIG_STATUS_DEPENDENCIES = $(newlib_basedir)/configure.host
