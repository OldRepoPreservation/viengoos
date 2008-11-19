# headers.m4 - Autoconf snippets to install links for header files.
# Copyright 2003, 2004, 2005 Free Software Foundation, Inc.
# Written by Marcus Brinkmann <marcus@gnu.org>.
#
# This file is free software; as a special exception the author gives
# unlimited permission to copy and/or distribute it, with or without
# modifications, as long as this notice is preserved.
#
# This file is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY, to the extent permitted by law; without even the
# implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

if test x$arch = xia32
then
  AC_CONFIG_LINKS([
    sysroot/include/l4.h:libl4/l4.h
    sysroot/include/l4/features.h:libl4/l4/features.h
    sysroot/include/l4/abi.h:libl4/l4/abi.h
    sysroot/include/l4/globals.h:libl4/l4/globals.h
    sysroot/include/l4/init.h:libl4/l4/init.h
    sysroot/include/l4/ipc.h:libl4/l4/ipc.h
    sysroot/include/l4/kip.h:libl4/l4/kip.h
    sysroot/include/l4/math.h:libl4/l4/math.h
    sysroot/include/l4/message.h:libl4/l4/message.h
    sysroot/include/l4/misc.h:libl4/l4/misc.h
    sysroot/include/l4/arch.h:libl4/l4/arch.h
    sysroot/include/l4/pagefault.h:libl4/l4/pagefault.h
    sysroot/include/l4/schedule.h:libl4/l4/schedule.h
    sysroot/include/l4/space.h:libl4/l4/space.h
    sysroot/include/l4/stubs-init.h:libl4/l4/stubs-init.h
    sysroot/include/l4/stubs.h:libl4/l4/stubs.h
    sysroot/include/l4/syscall.h:libl4/l4/syscall.h
    sysroot/include/l4/thread.h:libl4/l4/thread.h
    sysroot/include/l4/thread-start.h:libl4/l4/thread-start.h
    sysroot/include/l4/types.h:libl4/l4/types.h
    sysroot/include/l4/vregs.h:libl4/l4/vregs.h
    sysroot/include/l4/sigma0.h:libl4/l4/sigma0.h
    sysroot/include/l4/compat/ipc.h:libl4/l4/compat/ipc.h
    sysroot/include/l4/compat/kip.h:libl4/l4/compat/kip.h
    sysroot/include/l4/compat/message.h:libl4/l4/compat/message.h
    sysroot/include/l4/compat/misc.h:libl4/l4/compat/misc.h
    sysroot/include/l4/compat/schedule.h:libl4/l4/compat/schedule.h
    sysroot/include/l4/compat/space.h:libl4/l4/compat/space.h
    sysroot/include/l4/compat/syscall.h:libl4/l4/compat/syscall.h
    sysroot/include/l4/compat/thread.h:libl4/l4/compat/thread.h
    sysroot/include/l4/compat/types.h:libl4/l4/compat/types.h
    sysroot/include/l4/compat/sigma0.h:libl4/l4/compat/sigma0.h
    sysroot/include/l4/gnu/ipc.h:libl4/l4/gnu/ipc.h
    sysroot/include/l4/gnu/kip.h:libl4/l4/gnu/kip.h
    sysroot/include/l4/gnu/math.h:libl4/l4/gnu/math.h
    sysroot/include/l4/gnu/message.h:libl4/l4/gnu/message.h
    sysroot/include/l4/gnu/misc.h:libl4/l4/gnu/misc.h
    sysroot/include/l4/gnu/pagefault.h:libl4/l4/gnu/pagefault.h
    sysroot/include/l4/gnu/schedule.h:libl4/l4/gnu/schedule.h
    sysroot/include/l4/gnu/space.h:libl4/l4/gnu/space.h
    sysroot/include/l4/gnu/syscall.h:libl4/l4/gnu/syscall.h
    sysroot/include/l4/gnu/thread.h:libl4/l4/gnu/thread.h
    sysroot/include/l4/gnu/thread-start.h:libl4/l4/gnu/thread-start.h
    sysroot/include/l4/gnu/types.h:libl4/l4/gnu/types.h
    sysroot/include/l4/gnu/sigma0.h:libl4/l4/gnu/sigma0.h
    sysroot/include/l4/bits/ipc.h:libl4/${arch}/l4/bits/ipc.h
    sysroot/include/l4/bits/kip.h:libl4/${arch}/l4/bits/kip.h
    sysroot/include/l4/bits/math.h:libl4/${arch}/l4/bits/math.h
    sysroot/include/l4/bits/misc.h:libl4/${arch}/l4/bits/misc.h
    sysroot/include/l4/bits/arch.h:libl4/${arch}/l4/bits/arch.h
    sysroot/include/l4/bits/space.h:libl4/${arch}/l4/bits/space.h
    sysroot/include/l4/bits/stubs.h:libl4/${arch}/l4/bits/stubs.h
    sysroot/include/l4/bits/stubs-init.h:libl4/${arch}/l4/bits/stubs-init.h
    sysroot/include/l4/bits/syscall.h:libl4/${arch}/l4/bits/syscall.h
    sysroot/include/l4/bits/types.h:libl4/${arch}/l4/bits/types.h
    sysroot/include/l4/bits/vregs.h:libl4/${arch}/l4/bits/vregs.h
    sysroot/include/l4/bits/compat/ipc.h:libl4/${arch}/l4/bits/compat/ipc.h
    sysroot/include/l4/bits/compat/misc.h:libl4/${arch}/l4/bits/compat/misc.h
    sysroot/include/l4/bits/compat/arch.h:libl4/${arch}/l4/bits/compat/arch.h
    sysroot/include/l4/bits/compat/space.h:libl4/${arch}/l4/bits/compat/space.h
    sysroot/include/l4/bits/gnu/ipc.h:libl4/${arch}/l4/bits/gnu/ipc.h
    sysroot/include/l4/bits/gnu/kip.h:libl4/${arch}/l4/bits/gnu/kip.h
    sysroot/include/l4/bits/gnu/misc.h:libl4/${arch}/l4/bits/gnu/misc.h
    sysroot/include/l4/bits/gnu/arch.h:libl4/${arch}/l4/bits/gnu/arch.h
    sysroot/include/l4/bits/gnu/space.h:libl4/${arch}/l4/bits/gnu/space.h
    sysroot/include/l4/abi/kip.h:libl4/${l4_abi}/l4/abi/kip.h
    sysroot/include/l4/abi/abi.h:libl4/${l4_abi}/l4/abi/abi.h
  ])
  
  if test x$l4_abi = xv2
  then
    AC_CONFIG_LINKS([
      sysroot/include/l4/abi/bits/kip.h:libl4/v2/${arch}/l4/abi/bits/kip.h
    ])
  fi
  
  if test x$l4_abi = xx2
  then
    # AC_CONFIG_LINKS([])
    :
  fi
fi