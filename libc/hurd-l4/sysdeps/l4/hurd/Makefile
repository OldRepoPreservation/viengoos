# Copyright (C) 1993,94,95,96,97,98,99,2000,2001,2002, 2003, 2004
#	Free Software Foundation, Inc.
# This file is part of the GNU C Library.

# The GNU C Library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public
# License as published by the Free Software Foundation; either
# version 2.1 of the License, or (at your option) any later version.

# The GNU C Library is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Lesser General Public License for more details.

# You should have received a copy of the GNU Lesser General Public
# License along with the GNU C Library; if not, write to the Free
# Software Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
# 02111-1307 USA.

ifeq ($(subdir),csu)
sysdep_routines += errno-loc mremap
endif

ifeq ($(subdir),posix)
sysdep_routines += exit-thread
endif

ifdef in-Makerules

# We use the style `if (err = call(...))' a lot in the Hurd code,
# where we have a lot of functions that return zero or an errno code.
+cflags += -Wno-parentheses

# Do not use any assembly code from sysdeps/unix (and subdirectories).
# This bypasses all the system call stubs and uses any existing posix or
# generic C files instead.
inhibit-sysdep-asm += unix*
inhibit-unix-syscalls = yes

# Don't try to generate anything from the installed Unix system and its
# libraries.  That is only of use when building for a Unix system, so as to
# be compatible with some existing binaries for that system.
inhibit-glue = yes


ifeq ($(subdir),csu)

extra-objs += static-start.o

# We need special startup code for statically linked binaries.
$(objpfx)crt0.o: $(objpfx)static-start.o $(objpfx)abi-note.o $(objpfx)init.o
	$(link-relocatable)

endif

endif	# in-Makerules
