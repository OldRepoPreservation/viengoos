# hurd_SYSDEPS
# Written by Neal H Walfield <neal@walfield.org>.
#
# Find and link the header files appropriate for a given
# configuration.
#
# This function takes two arguments.  The first is a list of paths
# (relative to the ${srcdir}, i.e. the location of the configure
# script the macro is called from) to search for the header files.
#
# The second parameter is a list of header files to find and install.
# Each element may optionally contain a source and destination.  For
# instance, if the file to look for is foo.h, however, it should be
# installed as bar/foo.h, it should be listed as foo.h:bar/foo.h.
#
# For instance, hurd_SYSDEPS([sysdeps/$KERNEL/$ARCH
#                             sysdeps/$KERNEL
#                             sysdeps/generic
#                             sysdeps/$ARCH],
#                            [foo.h:hurd/foo.h
#                             bits/foo.h:hurd/bits/foo.h
#                             bar.h])
AC_DEFUN([hurd_SYSDEPS],
[AC_REQUIRE([AC_PROG_AWK])
 for ac_headers in `echo "$2"`
 do
   ac_SRC=`echo ${ac_headers} \
           | ${AWK} 'BEGIN { FS = ":" } { print $dnl
1; }'`
   ac_DEST=`echo ${ac_headers} \
           | ${AWK} 'BEGIN { FS = ":" } { if (NF == 1) print $dnl
1;  else print $dnl
2 }'`

   ac_header_ok=0
   for ac_dirs in `echo "$1"`
   do
     if test -e "${srcdir}/${ac_dirs}/${ac_SRC}"
     then
       AC_CONFIG_LINKS("include/${ac_DEST}":"${ac_dirs}/${ac_SRC}")
       ac_header_ok=1
       break
     fi
   done
   if test ${ac_header_ok} -ne 1
   then
     AC_MSG_ERROR([The header file ${ac_SRC} is required by, but not dnl
provided for this configuration.  Report this to the maintainer.])
   fi
 done])
  
