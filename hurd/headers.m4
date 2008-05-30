# headers.m4 - Autoconf snippets to install links for header files.
# Copyright 2003, 2007, 2008 Free Software Foundation, Inc.
# Written by Marcus Brinkmann <marcus@gnu.org>.
#
# This file is free software; as a special exception the author gives
# unlimited permission to copy and/or distribute it, with or without
# modifications, as long as this notice is preserved.
#
# This file is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY, to the extent permitted by law; without even the
# implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

AC_CONFIG_LINKS([sysroot/include/hurd/stddef.h:hurd/stddef.h
		 sysroot/include/hurd/types.h:hurd/types.h
		 sysroot/include/hurd/startup.h:hurd/startup.h
		 sysroot/include/hurd/addr.h:hurd/addr.h
		 sysroot/include/hurd/addr-trans.h:hurd/addr-trans.h
		 sysroot/include/hurd/cap.h:hurd/cap.h
		 sysroot/include/hurd/folio.h:hurd/folio.h
		 sysroot/include/hurd/rpc.h:hurd/rpc.h
		 sysroot/include/hurd/exceptions.h:hurd/exceptions.h
		 sysroot/include/hurd/thread.h:hurd/thread.h
		 sysroot/include/hurd/activity.h:hurd/activity.h
		 sysroot/include/hurd/lock.h:hurd/lock.h
		 sysroot/include/hurd/mutex.h:hurd/mutex.h
		 sysroot/include/hurd/rmutex.h:hurd/rmutex.h
		 sysroot/include/hurd/futex.h:hurd/futex.h
		 sysroot/include/hurd/error.h:hurd/error.h
		])
