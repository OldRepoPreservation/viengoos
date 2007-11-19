# headers.m4 - Autoconf snippets to install links for header files.
# Copyright 2004, 2005, 2007 Free Software Foundation, Inc.
# Written by Neal H. Walfield <neal@gnu.org>.
#
# This file is free software; as a special exception the author gives
# unlimited permission to copy and/or distribute it, with or without
# modifications, as long as this notice is preserved.
#
# This file is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY, to the extent permitted by law; without even the
# implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

AC_CONFIG_LINKS([
  include/hurd/mm.h:libhurd-mm/mm.h
  include/hurd/as.h:libhurd-mm/as.h
  include/hurd/storage.h:libhurd-mm/storage.h
  include/hurd/capalloc.h:libhurd-mm/capalloc.h
  include/hurd/pager.h:libhurd-mm/pager.h
  include/hurd/anonymous.h:libhurd-mm/anonymous.h
])
