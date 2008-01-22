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

noinst_LIBRARIES = libc.a libm.a

# Patches to apply to newlib's source tree.
patch_files = 00-newlib-configure-ac.patch \
	01-newlib-libc-sys-configure-in.patch \
	02-newlib-newlib-libc-string-makefile-am.patch \
	03-newlib-newlib-configure-host.patch \
	04-newlib-newlib-configure-ac.patch

# Files to link into newlib's source tree (relative to $(srcdir)/addon)
addon = newlib/libc/sys/hurd/configure.ac \
	newlib/libc/sys/hurd/configure \
	newlib/libc/sys/hurd/Makefile.am \
	newlib/libc/sys/hurd/Makefile.in \
	newlib/libc/sys/hurd/aclocal.m4 \
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
	newlib/libc/sys/hurd/getreent.c \
	newlib/libc/sys/hurd/malloc.c \
	newlib/libc/sys/hurd/mallocr.c \
	newlib/libc/sys/hurd/lock.c \
	newlib/libc/sys/hurd/crt0.c \
	newlib/libc/sys/hurd/include/pthread.h \
	newlib/libc/sys/hurd/include/assert.h \
	newlib/libc/sys/hurd/sys/lock.h \
	newlib/libc/sys/hurd/sys/config.h

EXTRA_DIST = $(addprefix patches/, $(patch_files)) \
	$(addprefix addon/, $(addon))


# The GNU C library version that we build against.
newlib_tarball = newlib-1.16.0.tar.gz
newlib_snapshot = ftp://sources.redhat.com/pub/newlib/$(newlib_tarball)

# Grab the snapshot
$(srcdir)/$(newlib_tarball):
	wget $(newlib_snapshot) -O $(srcdir)/$(newlib_tarball)

# Extract it.
$(srcdir)/newlib $(srcdir)/newlib-stamp: $(srcdir)/$(newlib_tarball)
	if test -e $(srcdir)/newlib; then \
		echo "Refusing to extract over $(srcdir)/newlib"; \
		exit 1; \
	fi
	( cd $(srcdir) \
		&& tar xfvz $(newlib_tarball) \
		&& mv $(subst .tar.gz,,$(newlib_tarball)) newlib )
	touch $(srcdir)/newlib
	touch $(srcdir)/newlib-stamp

# Apply any local patches.  (Revert any applied older version.)
$(addsuffix -applied,$(patch_files)): $(srcdir)/newlib-stamp
$(addsuffix -applied,$(patch_files)): %-applied: patches/%
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

abs_top_builddir := $(abspath $(top_builddir))
abs_srcdir := $(abspath $(srcdir))
abs_top_srcdir := $(abspath $(top_srcdir))

# Link the addon files into the source tree.
$(addprefix $(srcdir)/newlib/, $(addon)): $(srcdir)/newlib-stamp
$(addprefix $(srcdir)/newlib/, $(addon)): $(srcdir)/newlib/%: $(srcdir)/addon/%
	mkdir -p $(dir $@)
	$(LN_S) -f $(abspath $<) $@

newlib-configure-stamp: $(addsuffix -applied,$(patch_files)) \
		$(addprefix $(srcdir)/newlib/, $(addon))
	( cd $(srcdir)/newlib/newlib && autoreconf )
	rm -rf newlib-build
	mkdir newlib-build
	(cd newlib-build && $(abs_srcdir)/newlib/newlib/configure \
	--enable-maintainer-mode \
	CPPFLAGS=-I$(abs_top_builddir)/include\ -I$(abs_top_srcdir)/libc-parts \
	CFLAGS=-nostdinc\ -nostdlib \
	--disable-shared --disable-newlib-iconv --disable-multilib \
	--enable-newlib-hw-fp --enable-newlib-io-pos-args \
	--enable-newlib-io-c99-formats --enable-newlib-io-long-long \
	--enable-newlib-io-long-double \
	--enable-newlib-multithread \
	--prefix=$(abs_top_builddir)/newlib/newlib-install \
	--build=$(build_alias) --host=$(host_alias))
	touch newlib-configure-stamp

newlib-build/libc.a newlib-build/libc.m: newlib-configure-stamp
	cd newlib-build && make all install

libc.a: newlib-build/libc.a
	ln -sf newlib-build/libc.a libc.a
libm.a: newlib-build/libm.a
	ln -sf newlib-build/libm.a libm.a

all: include
include: newlib-configure-stamp
	ln -sf newlib-install/$(host_alias)/include include

clean-local:
	rm -rf newlib-build
	rm -rf newlib-install
	rm -rf $(srcdir)/newlib
CLEANFILES = newlib-configure-stamp \
	$(srcdir)/newlib-stamp $(addsuffix -applied,$(patch_files)) \
	include

DISTCLEANFILES = $(srcdir)/$(newlib_tarball) \
	$(srcdir)/newlib-configure-stamp
