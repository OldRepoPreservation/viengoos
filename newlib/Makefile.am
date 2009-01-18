# Makefile.am - Makefile template for newlib's libc
# Copyright (C) 2003, 2005, 2008 Free Software Foundation, Inc.
# Written by Neal H. Walfield
#
# This file is part of the GNU Hurd.
# 
# The GNU Hurd is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
# 
# The GNU Hurd is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
# 
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA

include $(top_srcdir)/Makefoo.am

# Remark: given that the newlib build system doesn't do any form of dependency
# tracking, it is correct to completely rebuild everything as soon as one of
# the $patch_files or $addon files changes.  Also it may be necessary to force
# a complete rebuild if one of the header files the newlib build depends on
# changes.  This is currently not being tracked in here.

# Patches to apply to newlib's source tree.
patch_files = \
	01-newlib-libc-sys-configure-in.patch \
	02-newlib-newlib-libc-string-makefile-am.patch \
	03-newlib-newlib-configure-host.patch \
	04-newlib-newlib-configure-ac.patch \
	05-newlib-newlib-libc-include-machine-setjmp-h.patch \
	06-newlib-newlib-libc-machine-i386-i386mach-h.patch \
	07-newlib-newlib-libm-machine-i386-i386mach-h.patch \
	08-new-newlib-libc-include-inttypes-h.patch

# Files to link into newlib's source tree (relative to $(srcdir)/addon)
addon = \
	newlib/libc/sys/hurd/configure.ac \
	newlib/libc/sys/hurd/Makefile.am \
	newlib/libc/sys/hurd/closer.c \
	newlib/libc/sys/hurd/reent.c \
	newlib/libc/sys/hurd/fcntlr.c \
	newlib/libc/sys/hurd/fstatr.c \
	newlib/libc/sys/hurd/gettimeofdayr.c \
	newlib/libc/sys/hurd/linkr.c \
	newlib/libc/sys/hurd/lseekr.c \
	newlib/libc/sys/hurd/openr.c \
	newlib/libc/sys/hurd/readr.c \
	newlib/libc/sys/hurd/renamer.c \
	newlib/libc/sys/hurd/signalr.c \
	newlib/libc/sys/hurd/sbrkr.c \
	newlib/libc/sys/hurd/statr.c \
	newlib/libc/sys/hurd/timesr.c \
	newlib/libc/sys/hurd/unlinkr.c \
	newlib/libc/sys/hurd/writer.c \
	newlib/libc/sys/hurd/isatty.c \
	newlib/libc/sys/hurd/getreent.c \
	newlib/libc/sys/hurd/malloc.c \
	newlib/libc/sys/hurd/mallocr.c \
	newlib/libc/sys/hurd/lock.c \
	newlib/libc/sys/hurd/crt0.c \
	newlib/libc/sys/hurd/include/pthread.h \
	newlib/libc/sys/hurd/include/assert.h \
	newlib/libc/sys/hurd/include/signal.h \
	newlib/libc/sys/hurd/sys/lock.h \
	newlib/libc/sys/hurd/sys/config.h \
	newlib/libc/sys/hurd/sys/errno.h \
	newlib/libc/sys/hurd/access.c \
	newlib/libc/sys/hurd/fsync.c \
	newlib/libc/sys/hurd/ftruncate.c \
	newlib/libc/sys/hurd/getcwd.c \
	newlib/libc/sys/hurd/getpwuid.c \
	newlib/libc/sys/hurd/getrusage.c \
	newlib/libc/sys/hurd/getuid.c \
	newlib/libc/sys/hurd/sleep.c \
	newlib/libc/sys/hurd/fd.h \
	newlib/libc/sys/hurd/pipefile.c \
	newlib/libc/sys/hurd/memfile.c

EXTRA_DIST = $(addprefix patches/, $(patch_files)) \
	$(addprefix addon/, $(addon))


# The newlib C library version that we build against.
newlib_tarball = newlib-1.16.0.tar.gz
newlib_snapshot = ftp://sources.redhat.com/pub/newlib/$(newlib_tarball)

# Grab the snapshot
$(srcdir)/$(newlib_tarball):
	wget $(newlib_snapshot) -O $(srcdir)/$(newlib_tarball)

# Extract it.
$(srcdir)/newlib/unpack.stamp: $(srcdir)/$(newlib_tarball)
	if test -d $(srcdir)/newlib/.; then \
		echo "Refusing to extract over $(srcdir)/newlib"; \
		exit 1; \
	fi
	( cd $(srcdir) \
		&& tar xfvz $(newlib_tarball) \
		&& mv $(subst .tar.gz,,$(newlib_tarball)) newlib )
	touch $@

# Apply any local patches.  (Revert any applied older version.)
$(addprefix $(srcdir)/newlib/,$(addsuffix .applied,$(patch_files))): $(srcdir)/newlib/unpack.stamp
$(addprefix $(srcdir)/newlib/,$(addsuffix .applied,$(patch_files))): $(srcdir)/newlib/%.applied: $(srcdir)/patches/%
	if test -e "$@"; \
	then \
	  ( cd $(srcdir)/newlib && sh /dev/stdin -R ) < $@ && rm -f $@; \
	fi
	( cd $(srcdir)/newlib && sh /dev/stdin ) < $<
	cp $< $@

rpatch:
	for p in $(shell tac -s ' ' $(patch_files)) ; do \
		(cd $(srcdir)/newlib; sh ../$(srcdir)/$${p} -R) || exit 1 ; \
	done

# Link the addon files into the source tree.
$(addprefix $(srcdir)/newlib/, $(addon)): $(srcdir)/newlib/unpack.stamp
$(addprefix $(srcdir)/newlib/, $(addon)): $(srcdir)/newlib/%: $(srcdir)/addon/%
	mkdir -p $(dir $@)
	$(LN_S) -f $(abspath $<) $@
# Touch the file, as Make follows symlinks, the source file for sure is younger
# than the unpack.stamp and thus those files will get linked again and again.
	touch $@

$(srcdir)/newlib/patch.stamp: $(srcdir)/newlib/unpack.stamp \
  $(addprefix $(srcdir)/newlib/,$(addsuffix .applied,$(patch_files))) \
  $(addprefix $(srcdir)/newlib/,$(addon))
	cd $(srcdir)/newlib/ && \
	$(fix_config_guess_and_sub)
	cd $(srcdir)/newlib/newlib/ && \
	autoreconf
# PACKAGE_* are not supposed to be defined in such public header files.
	cd $(srcdir)/newlib/newlib/ && \
	$(SED) /PACKAGE_/d < newlib.hin > newlib.hin.tmp && \
	mv newlib.hin.tmp newlib.hin
	touch $@

newlib-build/configure.stamp: $(srcdir)/newlib/patch.stamp
	rm -rf newlib-build
	mkdir newlib-build
	(cd newlib-build && $(abs_srcdir)/newlib/newlib/configure \
	--enable-maintainer-mode \
	--disable-shared --disable-newlib-iconv --disable-multilib \
	--enable-newlib-hw-fp --enable-newlib-io-pos-args \
	--enable-newlib-io-c99-formats --enable-newlib-io-long-long \
	--enable-newlib-io-long-double \
	--enable-newlib-multithread \
	--prefix=$(abs_top_builddir)/newlib/newlib-install \
	--host=$(host_alias) CC=$(host_alias)-gcc \
	)
	touch $@

if ARCH_X86_64
  # Without -fPIC, when linking Viengoos against libc-routines, we get
  # errors of the following form:
  #
  #   ../newlib/libc-routines.a(lib_a-strcpy.o):(.eh_frame+0x20): 
  #         relocation truncated to fit: R_X86_64_32 against `.text'
  #
  # See, e.g., http://forge.csail.mit.edu/pipermail/mitgcm-support/2005-October/003505.html
  EXTRA_CFLAGS = -fPIC
else
  EXTRA_CFLAGS =
endif

.PHONY: newlib-build/build.stamp
newlib-build/build.stamp: newlib-build/configure.stamp
	cd newlib-build && \
	  make \
	    TARGET_CFLAGS='$(EXTRA_CFLAGS) $(USER_CPPFLAGS) $(USER_CFLAGS) $(USER_LDFLAGS)' \
	    all
	if test newlib-build/libc.a -nt newlib-build/build.stamp; then \
	  ( cd newlib-build \
	    && make \
	      TARGET_CFLAGS='$(USER_CPPFLAGS) $(USER_CFLAGS) $(USER_LDFLAGS)' \
	      install ); \
	  touch $@; \
	fi

libc.a: newlib-build/libc.a
	ln -sf newlib-build/libc.a libc.a
libm.a: newlib-build/libm.a
	ln -sf newlib-build/libm.a libm.a

all-local: newlib-build/build.stamp libc.a libm.a libc-routines.a

# The following routines (should be) fine to be used in non-libc-linked code as
# well, so there's no need in duplicating their code somewhere else.
#routines := \
#	memcpy memmove memset bcopy bzero memchr rawmemchr	\
#	strstr strtok strchr strrchr strchrnul strcpy stpcpy	\
#	strcat strcmp strncmp strlen strnlen			\
#	atoi atoll strtoimax strtoumax				\
#	htonl htons ntohl ntohs ffs
routines := \
	memcpy memmove memset \
	strcpy strcmp strncmp strlen
# We have to find all modules in the archive which contain the above functions.
routines_subexp := 's/^\($(shell echo $(routines) \
	| $(SED) -e 's/ \+/\\|/g')\) in \(.*\)$$/\2/p'
libc-routines.a: libc.a Makefile
	rm -rf '$@'.d
	mkdir '$@'.d
	{ $(NM) --print-armap '$<' \
	  || echo CHOKE/ME; } | \
	  { $(SED) -n -e $(routines_subexp) \
	    || echo CHOKE/ME/2; } | \
	    { $(SORT) -u \
	      || echo CHOKE/ME/3; } | \
	      ( cd '$@'.d/ && \
	        xargs $(AR) -x ../'$<' \
	      )
	$(AR) qc '$@'.new '$@'.d/*
	rm -rf '$@'.d
	$(RANLIB) '$@'.new
	mv -f '$@'.new '$@'

all: include
include: newlib-build/configure.stamp
	ln -sf newlib-install/$(host_alias)/include include

#TODO.
clean-local:
	rm -rf newlib-build
	rm -rf newlib-install
	rm -rf $(srcdir)/newlib
	rm -rf libc-routines.a libc-routines.a.d
CLEANFILES = newlib-configure-stamp \
	$(srcdir)/newlib-stamp $(addsuffix -applied,$(patch_files)) \
	include

DISTCLEANFILES = $(srcdir)/$(newlib_tarball) \
	$(srcdir)/newlib-configure-stamp
