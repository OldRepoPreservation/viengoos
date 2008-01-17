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

# Files to link into newlib's source tree (relative to $(srcdir)/addon)
addon = 

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

# Link the addon files into the source tree.
$(addprefix $(srcdir)/newlib/, $(addon)): $(srcdir)/newlib-stamp
$(addprefix $(srcdir)/newlib/, $(addon)): $(srcdir)/newlib/%: $(srcdir)/addon/%
	mkdir -p $(dir $@)
	$(LN_S) -f $(abspath $<) $@

newlib-configure-stamp: $(addsuffix -applied,$(patch_files)) \
		$(addprefix $(srcdir)/newlib/, $(addon))
	rm -rf newlib-build
	mkdir newlib-build
	(cd newlib-build && $(abs_srcdir)/newlib/newlib/configure \
	--enable-maintainer-mode \
	CPPFLAGS=-I$(abs_top_builddir)/include CFLAGS=-nostdinc\ -nostdlib \
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
	rm -rf $(srcdir)/newlib
CLEANFILES = newlib-configure-stamp \
	$(srcdir)/newlib-stamp $(addsuffix -applied,$(patch_files))

DISTCLEANFILES = $(srcdir)/$(newlib_tarball) \
	$(srcdir)/newlib-configure-stamp
