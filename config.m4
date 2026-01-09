PHP_ARG_ENABLE(phpgo, whether to enable phpgo support,
[  --enable-phpgo           Enable phpgo support])

if test "$PHP_PHPGO" != "no"; then
  PHP_NEW_EXTENSION(phpgo, src/c/php_phpgo.c, $ext_shared)
  
  # Link our static lib manually to ensure it's included
  PHPGO_LIB_DIR="$abs_srcdir/go-build"
  PHP_ADD_LIBRARY_WITH_PATH(phpgo, $PHPGO_LIB_DIR, PHPGO_SHARED_LIBADD)
  
  # Go specific dependencies
  PHP_ADD_LIBRARY(pthread)
  PHP_ADD_LIBRARY(dl)
  
  PHP_SUBST(PHPGO_SHARED_LIBADD)
fi
