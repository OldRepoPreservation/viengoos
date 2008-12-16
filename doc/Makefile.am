# Makefile.am - Makefile template for the manual.
# Copyright (C) 2003, 2008 Free Software Foundation, Inc.
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

pkgdata_DATA = reference-guide.pdf

EXTRA_DIST=${fig_files} ${eps_files} ${pdf_files} ${tex_files} \
	reference-guide.pdf

tex_files = reference-guide.tex viengoos.tex runtime.tex
bib_files = bib.bib

fig_files =

eps_files=$(patsubst %.fig,%.eps,${fig_files})
eps_cleanfiles=${eps_files}

svg_files = 

pdf_files = $(patsubst %.svg,%.pdf,${svg_files})

%.pdf: %.svg
	$(INKSCAPE) --without-gui --export-area-drawing $< --export-pdf=$@

# _DEPENDENCIES does not work with _DATA.
reference-guide.pdf: ${eps_files} ${pdf_files} ${tex_files} ${bib_files}

SUFFIXES = .tex .eps .fig .pdf

%.pdf: %.tex
	TEXINPUTS="$(srcdir):$(TEXINPUTS)" $(PDFLATEX) \
		\\nonstopmode\\input $< -o $@ ; \
	BIBINPUTS="$(srcdir):$(BIBINPUTS)" $(BIBTEX) $* ; \
	TEXINPUTS="$(srcdir):$(TEXINPUTS)" $(PDFLATEX) \
		\\nonstopmode\\input $< -o $@ ; \
	TEXINPUTS="$(srcdir):$(TEXINPUTS)" $(PDFLATEX) \
		\\nonstopmode\\input $< -o $@ ;

.fig.eps:
	$(FIG2DEV) -L eps $< $@

clean:
	rm -f *.aux *.ps *.dvi *.pdf *.log *.toc *.bbl *.brf *.blg *.out

MAINTAINERCLEANFILES=${eps_cleanfiles}
