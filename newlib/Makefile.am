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
patch_files = 

patches = $(addprefix patches/, $(patch_files))

# Files to link into newlib's source tree (relative to $(srcdir)/addon)
addon = 

EXTRA_DIST = $(patches) $(addon)


# The GNU C library version that we build against.
newlib_tarball = newlib-1.16.0.tar.gz
newlib_snapshot = ftp://sources.redhat.com/pub/newlib/$(newlib_tarball)

# Grab the snapshot
$(srcdir)/$(newlib_tarball):
	wget $(newlib_snapshot) -O $(srcdir)/$(newlib_tarball)

# Extract it.  We can't use the directory's timestamp as that will be
# set by tar.
$(srcdir)/newlib-stamp: $(srcdir)/$(newlib_tarball)
	if test -e $(srcdir)/newlib; then \
		echo "Refusing to extract over $(srcdir)/newlib"; \
		exit 1; \
	fi
	( cd $(srcdir) \
		&& tar xfvz $(newlib_tarball) \
		&& mv $(subst .tar.gz,,$(newlib_tarball)) newlib )
	touch $(srcdir)/newlib-stamp

# Apply any local patches.
$(srcdir)/newlib-patch-stamp: $(srcdir)/newlib-stamp
	for p in $(patches) ; do \
		(cd $(srcdir)/newlib; sh ../$(srcdir)/$${p}) || exit 1 ; \
	done
	touch $(srcdir)/newlib-patch-stamp

rpatch:
	for p in $(shell tac -s ' ' $(patches)) ; do \
		(cd $(srcdir)/newlib; sh ../$(srcdir)/$${p} -R) || exit 1 ; \
	done
	rm $(srcdir)/newlib-patch-stamp

abs_top_builddir := $(abspath $(top_builddir))
abs_srcdir := $(abspath $(srcdir))

$(addprefix $(srcdir)/newlib/, $(addon)): $(srcdir)/newlib/%: $(srcdir)/addon/%
	mkdir -p $(dir $@)
	test -e $(srcdir)/addon/$${f}
	$(LN_S) $(abspath $<) $@

newlib-configure-stamp: $(srcdir)/newlib-patch-stamp \
		$(addprefix $(srcdir)/newlib/, $(addon))
	rm -rf newlib-build
	mkdir newlib-build
	(cd newlib-build && $(abs_srcdir)/newlib/newlib/configure \
	CPPFLAGS=-I$(abs_top_builddir)/include CFLAGS=-nostdlib\ -nostdinc \
	--disable-shared --disable-newlib-iconv --disable-multilib \
	--enable-newlib-hw-fp --enable-newlib-io-pos-args \
	--enable-newlib-io-c99-formats --enable-newlib-io-long-long \
	--enable-newlib-io-long-double \
	--prefix=$(prefix) --build=$(build_alias) --host=$(host_alias))
	touch newlib-configure-stamp

newlib-build/libc.a newlib-build/libc.m: newlib-configure-stamp
	cd newlib-build && make

libc.a: newlib-build/libc.a
	ln -sf newlib-build/libc.a libc.a
libm.a: newlib-build/libm.a
	ln -sf newlib-build/libm.a libm.a

clean-local:
	rm -rf newlib-build
CLEANFILES = newlib-configure-stamp

distclean-local:
	rm -rf $(srcdir)/newlib
DISTCLEANFILES = $(srcdir)/$(newlib_tarball) $(srcdir)/newlib-stamp \
	$(srcdir)/newlib-patch-stamp \
	$(srcdir)/newlib-configure-stamp
