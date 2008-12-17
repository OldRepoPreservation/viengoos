# headers.m4 - Autoconf snippets to install links for header files.
# Copyright 2008 Free Software Foundation, Inc.
# Written by Neal H. Walfield
#
# This file is free software; as a special exception the author gives
# unlimited permission to copy and/or distribute it, with or without
# modifications, as long as this notice is preserved.
#
# This file is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY, to the extent permitted by law; without even the
# implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

AC_CONFIG_LINKS([
  sysroot/include/viengoos.h:libviengoos/viengoos.h
  sysroot/include/viengoos/addr.h:libviengoos/viengoos/addr.h
  sysroot/include/viengoos/addr-trans.h:libviengoos/viengoos/addr-trans.h
  sysroot/include/viengoos/cap.h:libviengoos/viengoos/cap.h
  sysroot/include/viengoos/thread.h:libviengoos/viengoos/thread.h
  sysroot/include/viengoos/folio.h:libviengoos/viengoos/folio.h
  sysroot/include/viengoos/activity.h:libviengoos/viengoos/activity.h
  sysroot/include/viengoos/futex.h:libviengoos/viengoos/futex.h
  sysroot/include/viengoos/messenger.h:libviengoos/viengoos/messenger.h
  sysroot/include/viengoos/message.h:libviengoos/viengoos/message.h
  sysroot/include/viengoos/ipc.h:libviengoos/viengoos/ipc.h
  sysroot/include/viengoos/rpc.h:libviengoos/viengoos/rpc.h
  sysroot/include/viengoos/misc.h:libviengoos/viengoos/misc.h
])

