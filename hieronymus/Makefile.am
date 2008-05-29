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
boot_PROGRAMS = hieronymus

hieronymus_CPPFLAGS = $(USER_CPPFLAGS)
hieronymus_CFLAGS = $(USER_CFLAGS)
hieronymus_LDFLAGS = $(USER_LDFLAGS)
hieronymus_LDADD = $(USER_LDADD) $(addsuffix .o, $(files))

hieronymus_SOURCES = hieronymus.c modules.h

# List of modules to load followed by their priority, share, and then
# command line.  Each component is separated by a !
modules = ruth!1!100
ruth_commandline = "-D 3"

# List of directories relative to the top of the build tree to search
# for binaries.
module_paths = ruth

files = $(foreach module,$(modules),$(firstword $(subst !, ,$(module))))

modules.h: $(addsuffix .S, $(files)) Makefile
	echo '								\
	  $(foreach module,$(subst -,_,$(modules)),			\
	     extern char $(firstword $(subst !, ,$(module)))_start;	\
	     extern char $(firstword $(subst !, ,$(module)))_end;)	\
	  struct module modules[] = {					\
	    $(foreach module,$(subst -,_,$(modules)),			\
		{							\
	          STRINGIFY ( $(firstword $(subst !, ,$(module))) ),	\
		  $(wordlist 2, 2, $(subst !, ,$(module))),		\
		  $(wordlist 3, 3, $(subst !, ,$(module))),		\
		  $($(firstword $(subst !, ,$(module)))_commandline) "",\
	          &$(firstword $(subst !, ,$(module)))_start,		\
	          &$(firstword $(subst !, ,$(module)))_end		\
		},							\
	    )								\
	    };' > $@



hieronymus.c: modules.h

find_binary = \
	$(firstword \
	  $(wildcard \
	    $(addsuffix /$(1),$(addprefix $(top_builddir)/,$(module_paths)))) \
	      $(1):not_found)

binaries = $(foreach file,$(files),$(call find_binary,$(file)))

$(addsuffix .S, $(files)): %.S: module.S $(binaries) Makefile
	$(CPP) -DMODULE=$(subst -,_,$*) -DBINARY=$(call find_binary,$(@:.S=)) $< -o $@

$(addsuffix .o, $(files)): %.o: %.S
	$(AS) $< -o $@

