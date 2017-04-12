dnl $Id$
dnl config.m4 for extension easymoa

dnl Comments in this file start with the string 'dnl'.
dnl Remove where necessary. This file will not work
dnl without editing.

dnl If your extension references something external, use with:

PHP_ARG_WITH(easymoa, for easymoa support,
Make sure that the comment is aligned:
[  --with-easymoa             Include easymoa support])

dnl Otherwise use enable:

dnl PHP_ARG_ENABLE(easymoa, whether to enable easymoa support,
dnl Make sure that the comment is aligned:
dnl [  --enable-easymoa           Enable easymoa support])

if test "$PHP_EASYMOA" != "no"; then
  dnl Write more examples of tests here...

  dnl # --with-easymoa -> check with-path
  dnl SEARCH_PATH="/usr/local /usr"     # you might want to change this
  dnl SEARCH_FOR="/include/easymoa.h"  # you most likely want to change this
  dnl if test -r $PHP_EASYMOA/$SEARCH_FOR; then # path given as parameter
  dnl   EASYMOA_DIR=$PHP_EASYMOA
  dnl else # search default path list
  dnl   AC_MSG_CHECKING([for easymoa files in default path])
  dnl   for i in $SEARCH_PATH ; do
  dnl     if test -r $i/$SEARCH_FOR; then
  dnl       EASYMOA_DIR=$i
  dnl       AC_MSG_RESULT(found in $i)
  dnl     fi
  dnl   done
  dnl fi
  dnl
  dnl if test -z "$EASYMOA_DIR"; then
  dnl   AC_MSG_RESULT([not found])
  dnl   AC_MSG_ERROR([Please reinstall the easymoa distribution])
  dnl fi

  dnl # --with-easymoa -> add include path
  dnl PHP_ADD_INCLUDE($EASYMOA_DIR/include)

  dnl # --with-easymoa -> check for lib and symbol presence
  dnl LIBNAME=easymoa # you may want to change this
  dnl LIBSYMBOL=easymoa # you most likely want to change this 

  dnl PHP_CHECK_LIBRARY($LIBNAME,$LIBSYMBOL,
  dnl [
  dnl   PHP_ADD_LIBRARY_WITH_PATH($LIBNAME, $EASYMOA_DIR/$PHP_LIBDIR, EASYMOA_SHARED_LIBADD)
  dnl   AC_DEFINE(HAVE_EASYMOALIB,1,[ ])
  dnl ],[
  dnl   AC_MSG_ERROR([wrong easymoa lib version or lib not found])
  dnl ],[
  dnl   -L$EASYMOA_DIR/$PHP_LIBDIR -lm
  dnl ])
  dnl
  dnl PHP_SUBST(EASYMOA_SHARED_LIBADD)

  PHP_NEW_EXTENSION(easymoa, easymoa.c common.c, $ext_shared,, -DZEND_ENABLE_STATIC_TSRMLS_CACHE=1)
fi

dnl if test -z "$PHP_DEBUG"; then
dnl        AC_ARG_ENABLE(debug,
dnl                 [--enable-debug  compile with debugging system],
dnl                [PHP_DEBUG=$enableval], [PHP_DEBUG=no]
dnl         )
dnl fi
