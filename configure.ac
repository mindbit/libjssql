AC_PREREQ(2.61)
# Checks for programs.
AC_INIT([libjssql], [0.0.1], [support@mindbit.ro])

AC_CANONICAL_TARGET
AC_CANONICAL_HOST
AC_CANONICAL_SYSTEM

AM_INIT_AUTOMAKE
AC_CONFIG_HEADERS([jssql_config.h])
AC_CONFIG_MACRO_DIR([m4])
AC_PROG_CC
AC_PROG_INSTALL
AC_PROG_LN_S
AC_PROG_MAKE_SET
AM_PROG_CC_C_O
LT_INIT([disable-shared])
AC_SUBST(MKDIR_P)


CFLAGS="-Wall $CFLAGS"


# additional configure arguments

AC_ARG_WITH(js-include,
    AC_HELP_STRING([--with-js-include=DIR], [search for mozjs (spidermonkey) headers in DIR]),
    [
        JS_INCLUDE="$withval"
    ]
)

AC_ARG_WITH(js-lib,
    AC_HELP_STRING([--with-js-lib=NAME], [specify mozjs (spidermonkey) library name]),
    [
        JS_LIB="$withval"
    ]
)

AC_ARG_WITH(jsmisc,
    AC_HELP_STRING([--with-jsmisc=DIR], [search for jsmisc in DIR]),
    [
        JSMISC="$withval"
    ]
)


# test for SpiderMonkey 17.0 (mozjs 17.0)

if test -z "$JS_INCLUDE" ; then
    JS_INCLUDE=" \
        /usr/include/js-17.0 \
        /usr/local/include/js-17.0 \
    "
    AC_CHECK_HEADER(jsapi.h,[],[],-)
fi
for incpath in $JS_INCLUDE; do
    if test "x$ac_cv_header_jsapi_h" != xyes ; then
        AC_MSG_CHECKING([for jsapi.h in $incpath])
        save_cppflags="$CPPFLAGS"
        CPPFLAGS="-I$incpath $CPPFLAGS"
        AC_PREPROC_IFELSE([AC_LANG_PROGRAM([[
                #include <jsapi.h>
            ]])],
            [
                AC_MSG_RESULT(yes)
                ac_cv_header_jsapi_h=yes
            ],
            [
                AC_MSG_RESULT(no)
                CPPFLAGS="$save_cppflags"
            ]
        )
   fi
done
if test "x$ac_cv_header_jsapi_h" != xyes ; then
    AC_MSG_ERROR([jsapi.h not found])
fi

if test -z "$JS_LIB" ; then
    JS_LIB=" \
        mozjs-17.0 \
        mozjs \
    "
fi
for lib in $JS_LIB; do
    if test "x$js_lib_found" != xyes ; then
        AC_CHECK_LIB([$lib],[JS_NewContext],[
            LIBS="-l$lib $LIBS"
            js_lib_found=yes
        ])
    fi
done
if test "x$js_lib_found" != xyes ; then
    AC_MSG_ERROR([no usable mozjs (spidermonkey) library found])
fi


# test for jsmisc

if test -n "$JSMISC" ; then
    CPPFLAGS="-I$JSMISC $CPPFLAGS"
fi
AC_CHECK_HEADER(jsmisc.h,[],[],-)
if test "x$ac_cv_header_jsmisc_h" != xyes ; then
    AC_MSG_ERROR([jsmisc.h not found])
fi

AC_CHECK_LIB([jsmisc],[__js_log])


# test for mysql
mysql=false
mysqlfail=false
AC_MSG_CHECKING(for mysql)
AC_ARG_WITH(mysql,
  [  --with-mysql=PFX        base of MySQL installation],
  [AC_MSG_RESULT(mysql path = "$with_mysql")
   if test "$with_mysql" != "no"
   then
    if test "$with_mysql" != "yes"
    then
      LDFLAGS="-L$with_mysql/lib -L$with_mysql/lib/mysql $LDFLAGS"
      CFLAGS="-I$with_mysql/include $CFLAGS"
    fi
    AC_CHECK_HEADERS(mysql/mysql.h,
     AC_CHECK_LIB(mysqlclient, mysql_init,
      MYSQL_LDFLAGS="-L$with_mysql/lib -L$with_mysql/lib/mysql"
      MYSQL_CFLAGS="-I$with_mysql/include"

      mysql=true), [], [$CYGWIN_MYSQL_MAGIC])
   fi
  ],
  [AC_MSG_RESULT([--with-mysql not specified])
   if test -d "/usr/lib64/mysql"; then
    MYSQL_LIBDIR="/usr/lib64/mysql"
   elif test -d "/usr/lib/mysql"; then
    MYSQL_LIBDIR="/usr/lib/mysql"
   else
    MYSQL_LIBDIR="/usr/lib"
   fi
   LDFLAGS="-L$MYSQL_LIBDIR $LDFLAGS"
   LIBS="-lmysqlclient $LIBS"
   AC_CHECK_LIB(mysqlclient, mysql_init,
    [AC_CHECK_HEADERS(mysql/mysql.h,
      mysql=true

     , [], [$CYGWIN_MYSQL_MAGIC])])
  ])

# additional version check for mysql
AC_ARG_ENABLE(mysql-version-check, [  --disable-mysql-version-check  do not check MySQL version],, enable_mysql_version_check=yes)
if test "$mysql" = "true" -a "x$enable_mysql_version_check" = "xyes"
then
  AC_MSG_CHECKING(mysql version)
  AC_RUN_IFELSE([AC_LANG_PROGRAM(
    [[$CYGWIN_MYSQL_MAGIC
      #include <mysql/mysql.h>]],
    [[if (MYSQL_VERSION_ID < 40100)
        return(-1);
      else
        return(0);
    ]])
    ],mysql=true,mysql=false)
  if test "$mysql" = "false"
  then
    mysqlfail=true
    AC_MSG_RESULT([fail, >= 4.1 required])
  else
    AC_MSG_RESULT(ok)
  fi
fi
AM_CONDITIONAL(HAVE_MYSQL_MYSQL_H, test x$mysql = xtrue)
AM_CONDITIONAL(HAVE_MYSQL_MYSQL_H, test "0" = "1")

# test for postgres
postgres=false
AC_MSG_CHECKING(for postgres)
AC_ARG_WITH(postgres,
  [  --with-postgres=PFX       base of postgres installation],
  [AC_MSG_RESULT(postgres path = "$with_postgres")
   case $with_postgres in
   no)
     ;;
   yes)
    AC_CHECK_HEADERS(postgresql/libpq-fe.h,
     postgres=true)
     ;;
   *)
    LDFLAGS="-L$with_postgres/lib $LDFLAGS"
    CFLAGS="-I$with_postgres/include $CFLAGS"
    LIBS="-lpq $LIBS"
    AC_CHECK_HEADERS(postgresql/libpq-fe.h,
     postgres=true)
    ;;
   esac
  ],
  [AC_MSG_RESULT([--with-postgres not specified])
    AC_CHECK_HEADERS(postgresql/libpq-fe.h, 
      LIBS="-lpq $LIBS"
      LDFLAGS="-L/usr/include/postgresql $LDFLAGS"
	  CFLAGS="-I/usr/include/postgresql $CFLAGS"
      postgres=true)])
AM_CONDITIONAL(HAVE_POSTGRESQL_LIBPQ_FE_H, test x$postgres = xtrue)

# Debug mode

AC_ARG_ENABLE([debug],
    AS_HELP_STRING([--enable-debug], [Enable debugging mode]))

AS_IF([test "x$enable_debug" = "xyes"], [
    CFLAGS="$CFLAGS -DDEBUG"
])

# mysql
if test "x$mysql" = "xfalse"
then
  AC_MSG_NOTICE([NOTICE: MySQL not found.  MySQL support will not be compiled.])
  AM_CONDITIONAL(HAVE_MYSQL_MYSQL_H, false)
else
  # warn user if mysql found but not used due to version
  if test "$mysqlfail" = "true"
  then
    AC_MSG_NOTICE([NOTICE: MySQL found, but too old. MySQL support will not be compiled.])
    AM_CONDITIONAL(HAVE_MYSQL_MYSQL_H, false)
  else
    AM_CONDITIONAL(HAVE_MYSQL_MYSQL_H, true)
    AC_DEFINE([HAVE_MYSQL_MYSQL_H],[1],[Have mysql library])
  fi
fi

# postgres
if test "x$postgres" = "xfalse"
then
  AC_MSG_NOTICE([NOTICE: PostgreSQL not found.  PostgreSQL support will not be compiled.])
  AM_CONDITIONAL(HAVE_POSTGRESQL_LIBPQ_FE_H, false)
else
  AM_CONDITIONAL(HAVE_POSTGRESQL_LIBPQ_FE_H, true)
  AC_DEFINE([HAVE_POSTGRESQL_LIBPQ_FE_H],[1],[Have postgres library])
fi

AC_CONFIG_FILES([
Makefile
src/Makefile
])
AC_OUTPUT
