# headers.m4 - Autoconf snippets to install links for header files.
# Copyright 2003 Free Software Foundation, Inc.
# Written by Marcus Brinkmann <marcus@gnu.org>.
#
# This file is free software; as a special exception the author gives
# unlimited permission to copy and/or distribute it, with or without
# modifications, as long as this notice is preserved.
#
# This file is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY, to the extent permitted by law; without even the
# implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

AC_CONFIG_LINKS([
  include/l4.h:libl4/l4.h
  include/l4/globals.h:libl4/l4/globals.h
  include/l4/init.h:libl4/l4/init.h
  include/l4/ipc.h:libl4/l4/ipc.h
  include/l4/kip.h:libl4/l4/kip.h
  include/l4/math.h:libl4/l4/math.h
  include/l4/misc.h:libl4/l4/misc.h
  include/l4/schedule.h:libl4/l4/schedule.h
  include/l4/space.h:libl4/l4/space.h
  include/l4/stubs.h:libl4/l4/stubs.h
  include/l4/stubs-init.h:libl4/l4/stubs-init.h
  include/l4/syscall.h:libl4/l4/syscall.h
  include/l4/thread.h:libl4/l4/thread.h
  include/l4/types.h:libl4/l4/types.h
  include/l4/vregs.h:libl4/l4/vregs.h
  include/l4/compat/types.h:libl4/l4/compat/types.h
  include/l4/bits/ipc.h:libl4/${arch}/l4/bits/ipc.h
  include/l4/bits/kip.h:libl4/${arch}/l4/bits/kip.h
  include/l4/bits/math.h:libl4/${arch}/l4/bits/math.h
  include/l4/bits/misc.h:libl4/${arch}/l4/bits/misc.h
  include/l4/bits/space.h:libl4/${arch}/l4/bits/space.h
  include/l4/bits/stubs.h:libl4/${arch}/l4/bits/stubs.h
  include/l4/bits/stubs-init.h:libl4/${arch}/l4/bits/stubs-init.h
  include/l4/bits/syscall.h:libl4/${arch}/l4/bits/syscall.h
  include/l4/bits/types.h:libl4/${arch}/l4/bits/types.h
  include/l4/bits/vregs.h:libl4/${arch}/l4/bits/vregs.h
])
