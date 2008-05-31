# Makefile.am - Makefile template for the Boehm GC
# Copyright (C) 2008 Free Software Foundation, Inc.
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

noinst_LIBRARIES = libgc.a

# Patches to apply to the source tree.
patch_files = 01-gc-include-private-gcconfig-h.patch \
	02-gc-mach_dep-c.patch

# Files to link into newlib's source tree (relative to $(srcdir)/addon)
addon = 

EXTRA_DIST = $(addprefix patches/, $(patch_files)) \
	$(addprefix addon/, $(addon))

# The source tarball.
gc_tarball = gc-7.0.tar.gz
gc_snapshot = http://www.hpl.hp.com/personal/Hans_Boehm/gc/gc_source/$(gc_tarball)

# Grab the snapshot
$(srcdir)/$(gc_tarball):
	wget $(gc_snapshot) -O $(srcdir)/$(gc_tarball)

# Extract it.
$(srcdir)/gc/unpack.stamp: $(srcdir)/$(gc_tarball)
	if test -e $(srcdir)/gc/.; then \
		echo "Refusing to extract over $(srcdir)/gc"; \
		exit 1; \
	fi
	( cd $(srcdir) \
		&& tar xfvz $(gc_tarball) \
		&& mv $(subst .tar.gz,,$(gc_tarball)) gc )
	touch $@

# Apply any local patches.  (Revert any applied older version.)
$(addprefix $(srcdir)/gc/,$(addsuffix .applied,$(patch_files))): $(srcdir)/gc/unpack.stamp
$(addprefix $(srcdir)/gc/,$(addsuffix .applied,$(patch_files))): $(srcdir)/gc/%.applied: $(srcdir)/patches/%
	if test -e "$@"; \
	then \
	  ( cd $(srcdir)/gc && sh /dev/stdin -R ) < $@ && rm -f $@; \
	fi
	( cd $(srcdir)/gc && sh /dev/stdin ) < $<
	cp $< $@

rpatch:
	for p in $(shell tac -s ' ' $(patch_files)) ; do \
		(cd $(srcdir)/gc; sh ../$(srcdir)/$${p} -R) || exit 1 ; \
	done

# Link the addon files into the source tree.
$(addprefix $(srcdir)/gc/, $(addon)): $(srcdir)/gc/unpack.stamp
$(addprefix $(srcdir)/gc/, $(addon)): $(srcdir)/gc/%: $(srcdir)/addon/%
	mkdir -p $(dir $@)
	$(LN_S) -f $(abspath $<) $@
# Touch the file, as Make follows symlinks, the source file for sure is younger
# than the unpack.stamp and thus those files will get linked again and again.
	touch $@

$(srcdir)/gc/patch.stamp: $(srcdir)/gc/unpack.stamp \
  $(addprefix $(srcdir)/gc/,$(addsuffix .applied,$(patch_files))) \
  $(addprefix $(srcdir)/gc/,$(addon))
	cd $(srcdir)/gc/ \
		&& $(fix_config_guess_and_sub) \
		&& autoreconf
	touch $@

gc-build/configure-stamp: $(srcdir)/gc/patch.stamp Makefile.am
	rm -rf gc-build
	mkdir gc-build
	(cd gc-build && ../$(srcdir)/gc/configure \
	    CPPFLAGS="$(USER_CPPFLAGS)" \
	   CFLAGS='$(USER_CFLAGS)' \
	   LDFLAGS='$(USER_LDFLAGS)' \
	   EXTRA_TEST_LIBS='$(USER_LDADD)' \
	--disable-shared --disable-threads \
	--disable-java-finalization --disable-gcj-support \
	--prefix=$(abs_builddir)/gc-install \
	--build=$(build_alias) --host=$(host_alias))
	touch $@

gc-build/build-stamp: gc-build/configure-stamp
	cd gc-build && \
	  make all && \
	  make gctest
	touch $@

gc-install/install-stamp: gc-build/build-stamp
	cd gc-build && \
	  make install
	touch $@

libgc.a: gc-install/install-stamp
	$(LN_S) -f gc-install/lib/libgc.a .

clean-local:
	rm -rf gc-build
	rm -rf gc-install

CLEANFILES = include

DISTCLEANFILES = $(srcdir)/$(gc_tarball) \
	gc-configure-stamp

