# Makefile.am - Makefile for Hieronymus.
# Copyright (C) 2008 Free Software Foundation, Inc.
# Written by Neal H. Walfield.
#
# This file is part of the GNU Hurd.
# 
# The GNU Hurd is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 3 of the License, or
# (at your option) any later version.
#
# This GNU Hurd is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.

bootdir = $(prefix)/boot
if ! ENABLE_TESTS
boot_PROGRAMS = hieronymus

all-local: hieronymus.stripped
hieronymus.stripped: hieronymus
	strip -o $@ $<
endif

hieronymus_CPPFLAGS = $(USER_CPPFLAGS)
hieronymus_CFLAGS = $(USER_CFLAGS)
hieronymus_LDFLAGS = $(USER_LDFLAGS)
hieronymus_LDADD = $(USER_LDADD) $(addsuffix .o, $(files))

hieronymus_SOURCES = hieronymus.c modules.h

# List of modules to load followed by their priority, weight, and a
# delay (in seconds) at which to start the module.  Each component is
# separated by a !
modules = ruth!1!100
ruth_commandline = "-D 3"
#modules = activity-distribution!1!100
#modules = shared-memory-distribution
#modules = gcbench!1!200!0 gcbench2!1!400!120

# List of directories relative to the top of the build tree to search
# for binaries.
module_paths = ruth benchmarks

files = $(foreach module,$(modules),$(firstword $(subst !, ,$(module))))

find_binary = \
	$(firstword \
	  $(wildcard \
	    $(addsuffix /$(1),$(addprefix $(top_builddir)/,$(module_paths)))) \
	      $(1):not_found)

md5sum = \
	$(shell md5sum $(1) | awk '{ printf $$1 }' \
		| sed 's/\([0-9a-f]\{2\}\)/0x\1, /g')

tovar = $(subst -,_,$(firstword $(subst !, ,$(1))))

hieronymus.c: modules.h

binaries = $(foreach file,$(files),$(call find_binary,$(file)))

$(files): %: $(binaries) Makefile
	strip -o $@ $<

modules.h: $(files)
modules.h: $(addsuffix .S, $(files)) Makefile
	echo '								\
	  $(foreach module,$(subst -,_,$(modules)),			\
	     extern char $(call tovar,$(module))_start;	\
	     extern char $(call tovar,$(module))_end;)	\
	  struct module modules[] = {					\
	    $(foreach module,$(modules),				\
		{							\
	          "$(firstword $(subst !, ,$(module)))",		\
		  $(wordlist 2, 2, $(subst !, ,$(module))) + 0,		\
		  $(wordlist 3, 3, $(subst !, ,$(module))) + 0,		\
		  $(wordlist 4, 4, $(subst !, ,$(module))) + 0,		\
		  $($(call tovar,$(module))_commandline) "",		\
	          &$(call tovar,$(module))_start,			\
	          &$(call tovar,$(module))_end,				\
		  { $(call md5sum,$(firstword $(subst !, ,$(module)))) }\
		},							\
	    )								\
	    };' > $@


$(addsuffix .S, $(files)): %.S: module.S $(binaries) Makefile
	$(CPP) -DMODULE=$(subst -,_,$*) -DBINARY=$(@:.S=) $< -o $@

$(addsuffix .o, $(files)): %.o: %
$(addsuffix .o, $(files)): %.o: %.S
	$(CCAS) $(CCASFLAGS) -c -o $@ $<

CLEANFILES = modules.h hieronymus.striped $(files) $(addsuffix .S, $(files))