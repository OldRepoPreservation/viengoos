# Makefile.am - Makefile template for libc
# Copyright (C) 2003, 2005 Free Software Foundation, Inc.
# Written by Marcus Brinkmann.
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

noinst_LIBRARIES = libc.a

patch_files = 00-configure-touch.patch 01-configure.in-base-os.patch \
	02-configure.in-add-ons-generic.patch \
	03-sysdeps-generic-bits-socket-h.patch \
	04-sysdeps-generic-bits-sigcontext-h.patch \
	05-sysdeps-generic-getpeername.patch \
	06-sysdeps-generic-open.patch \
	07-sysdeps-generic-fcntl.patch \
	08-sysdeps-generic-dl-sysdep.patch \
	09-sysdeps-generic-lseek.patch \
	50-nptl-hurd-l4.patch \
	51-nptl-hurd-l4-tls.patch

patches = $(addprefix patches/, $(patch_files))

hurd_l4_sysdeps_l4_hurd = bits/errno.h bits/fcntl.h bits/local_lim.h \
	bits/mman.h bits/posix_opt.h bits/stat.h bits/typesizes.h \
	brk.c closedir.c dirfd.c dirstream.h enbl-secure.c \
	execve.c _exit.c exit-thread.c \
	fork.c fstatfs64.c fstatfs.c fstatvfs64.c fstatvfs.c fxstat64.c \
	fxstat.c _G_config.h getdtsz.c getegid.c geteuid.c \
	getppid.c Implies i386/init-first.c i386/static-start.S \
	isatty.c lxstat64.c lxstat.c Makeconfig Makefile \
	mremap.c opendir.c read.c readdir.c readdir_r.c rewinddir.c \
	sigaction.c sigprocmask.c \
	sys/param.h syscall.c telldir.c uname.c write.c xstat64.c xstat.c
hurd_l4_sysdeps_l4 = getpagesize.c sched_yield.c usleep.c \
	getclktck.c gettimeofday.c nanosleep.c sleep.c
hurd_l4 = configure configure.in Makeconfig Makefile \
	$(addprefix sysdeps/l4/,$(hurd_l4_sysdeps_l4)) \
	$(addprefix sysdeps/l4/hurd/,$(hurd_l4_sysdeps_l4_hurd))
hurd_l4_add_on = $(addprefix hurd-l4/, $(hurd_l4))


EXTRA_DIST = $(patches) $(hurd_l4_add_on) gcc-3.4.3-i686-gnu.dpatch


# The GNU C library version that we build against.
glibc_repository = :pserver:anoncvs@sources.redhat.com:/cvs/glibc
glibc_module = libc
glibc_version = 2005-01-23

libc-cvs:
	test ! -e libc || (echo "GNU C library already checked out." && false)
	cvs -z3 -d $(glibc_repository) \
		co -D $(glibc_version) -d libc $(glibc_module)

libc-checkout-stamp:
	test -e libc || (echo "You must setup the libc source (see README)." \
			&& false)
	touch libc-checkout-stamp

libc-patch-stamp: libc-checkout-stamp
	for p in $(patches) ; do \
		(cd libc; sh ../$(srcdir)/$${p}) || exit 1 ; \
	done
	touch libc-patch-stamp

# FIXME: Order should be reversed.
rpatch:
	for p in $(patches) ; do \
		(cd libc; sh ../$(srcdir)/$${p} -R) || exit 1 ; \
	done
	rm libc-patch-stamp

libc-add-on-stamp: libc-checkout-stamp
	for f in $(hurd_l4_add_on) ; do \
		install -D $(srcdir)/$${f} libc/$${f} ; \
	done
	touch libc-add-on-stamp

libc-configure-stamp: libc-patch-stamp libc-add-on-stamp
	test ! -e libc-build || (echo "Build dir already exists." && false)
	mkdir libc-build
	(cd libc-build && ../libc/configure --disable-shared \
	--disable-profile --without-cvs --enable-add-ons=nptl,hurd-l4 \
	--prefix=$(prefix) --build=$(build_alias) --host=$(host_alias))
	touch libc-configure-stamp

libc-build-stamp: libc-configure-stamp
	(cd libc-build; make)
	touch libc-build-stamp

libc.a: libc-build-stamp
	ln -sf libc-build/libc.a libc.a

# clean:
#	rm -fR libc.a libc-build
#
# distclean:
#	rm -fR libc