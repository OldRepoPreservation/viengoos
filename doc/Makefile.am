# Makefile.am - Makefile template for the manual.
# Copyright (C) 2003 Free Software Foundation, Inc.
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

pkgdata_DATA = hurd-on-l4.dvi hurd-on-l4.ps hurd-on-l4.pdf

# _DEPENDENCIES does not work with _DATA.
hurd-on-l4.dvi: \
	hurd-on-l4.tex \
	introduction.tex \
	booting.tex \
	ipc.tex \
	threads-tasks.tex \
	vmm.tex \
	authentication.tex \
	posix.tex \
	debugging.tex \
	device-drivers.tex

SUFFIXES = .ps .dvi .tex

.tex.dvi:
	$(LATEX) $< -o $@
	while egrep "^LaTeX Warning:.*Rerun to" \
	    `echo $< | sed -e 's/.tex$$/.log/'`; do \
	  rm -f $(subst $<,.tex,.log); \
	  $(LATEX) $< -o $@; \
	done

.dvi.ps:
	$(DVIPS) $<

.ps.pdf:
	$(PS2PDF) $<

clean:
	rm -f *.aux *.ps *.dvi *.pdf *.log *.toc

