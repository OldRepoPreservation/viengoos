# Makefile.am - Makefile template for Sqlite.
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

# Patches to apply to the source tree.
patch_files = 

# Files to link into newlib's source tree (relative to $(srcdir)/addon)
addon = 

EXTRA_DIST = $(addprefix patches/, $(patch_files)) \
	$(addprefix addon/, $(addon))

# The source tarball.
tarball = sqlite-3.6.4.tar.gz
snapshot = http://sqlite.org/$(tarball)

# Grab the snapshot
$(srcdir)/$(tarball):
	wget $(snapshot) -O $(srcdir)/$(tarball)

# Extract it.
$(srcdir)/src/unpack.stamp: $(srcdir)/$(tarball)
	if test -e $(srcdir)/src/.; then \
		echo "Refusing to extract over $(srcdir)/src"; \
		exit 1; \
	fi
	( cd $(srcdir) \
		&& tar xfvz $(tarball) \
		&& mv $(subst .tar.gz,,$(tarball)) src )
	touch $@

# Apply any local patches.  (Revert any applied older version.)
$(addprefix $(srcdir)/src/,$(addsuffix .applied,$(patch_files))): $(srcdir)/src/unpack.stamp
$(addprefix $(srcdir)/src/,$(addsuffix .applied,$(patch_files))): $(srcdir)/src/%.applied: $(srcdir)/patches/%
	if test -e "$@"; \
	then \
	  ( cd $(srcdir)/src && sh /dev/stdin -R ) < $@ && rm -f $@; \
	fi
	( cd $(srcdir)/src && sh /dev/stdin ) < $<
	cp $< $@

rpatch:
	for p in $(shell tac -s ' ' $(patch_files)) ; do \
		(cd $(srcdir)/src; sh ../$(srcdir)/$${p} -R) || exit 1 ; \
	done

# Link the addon files into the source tree.
$(addprefix $(srcdir)/src/, $(addon)): $(srcdir)/src/unpack.stamp
$(addprefix $(srcdir)/src/, $(addon)): $(srcdir)/src/%: $(srcdir)/addon/%
	mkdir -p $(dir $@)
	$(LN_S) -f $(abspath $<) $@
# Touch the file, as Make follows symlinks, the source file for sure is younger
# than the unpack.stamp and thus those files will get linked again and again.
	touch $@

$(srcdir)/src/patch.stamp: $(srcdir)/src/unpack.stamp \
  $(addprefix $(srcdir)/src/,$(addsuffix .applied,$(patch_files))) \
  $(addprefix $(srcdir)/src/,$(addon))
	cd $(srcdir)/src/ \
		&& $(fix_config_guess_and_sub) \
		&& autoreconf && aclocal --force && autoreconf
	touch $@

build/configure-stamp: $(srcdir)/src/patch.stamp Makefile.am
	rm -rf build
	mkdir build
	(cd build && ../$(srcdir)/src/configure \
	    CPPFLAGS="$(USER_CPPFLAGS)" \
	   CFLAGS='$(USER_CFLAGS)' \
	   LDFLAGS='$(USER_LDFLAGS)' \
	--disable-shared --disable-threadsafe \
	--enable-tempstore=always --disable-tcl --disable-readline \
	--disable-amalgamation --disable-load-extension --disable-gcov \
	--prefix=$(abs_builddir)/install \
	--build=$(build_alias) --host=$(host_alias))
	touch $@

all-local: build/configure-stamp
	cd build && \
	  make all && make install

clean-local:
	rm -rf build
	rm -rf install

CLEANFILES = include

DISTCLEANFILES = $(srcdir)/$(tarball) \
	build/configure-stamp

