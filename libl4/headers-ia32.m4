# headers-ia32.m4 - Autoconf snippets to install links for header files.
# Copyright 20013 Free Software Foundation, Inc.
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
  include/l4/bits/ipc.h:libl4/ia32/l4/bits/ipc.h
  include/l4/bits/kip.h:libl4/ia32/l4/bits/kip.h
  include/l4/bits/math.h:libl4/ia32/l4/bits/math.h
  include/l4/bits/misc.h:libl4/ia32/l4/bits/misc.h
  include/l4/bits/space.h:libl4/ia32/l4/bits/space.h
  include/l4/bits/stubs.h:libl4/ia32/l4/bits/stubs.h
  include/l4/bits/stubs-init.h:libl4/ia32/l4/bits/stubs-init.h
  include/l4/bits/syscall.h:libl4/ia32/l4/bits/syscall.h
  include/l4/bits/types.h:libl4/ia32/l4/bits/types.h
  include/l4/bits/vregs.h:libl4/ia32/l4/bits/vregs.h
])
