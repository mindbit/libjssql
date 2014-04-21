AC_PREREQ(2.61)
# Checks for programs.
AC_INIT([libjssql], [0.0,1], [support@mindbit.ro])

AC_CANONICAL_TARGET
AC_CANONICAL_HOST
AC_CANONICAL_SYSTEM

AM_INIT_AUTOMAKE
AC_CONFIG_HEADERS([jssql_config.h])
AC_CONFIG_MACRO_DIR([m4])
AC_PROG_CC
AC_PROG_CXX
AC_PROG_OBJC
AC_PROG_INSTALL
AC_PROG_LN_S
AC_PROG_MAKE_SET
AM_PROG_CC_C_O
LT_INIT([disable-shared])
AC_SUBST(MKDIR_P)


CFLAGS="-Wall $CFLAGS"

# Check system type
case "$host_os" in
linux*)
     AC_DEFINE_UNQUOTED(LINUX,1,[This is a Linux kernel])
     build_target="linux"
     LIBPREFIX=
     DLLDIR=lib
     UNIXONLY="#"
     AC_PATH_XTRA
     ;;
*cygwin*)
     AC_DEFINE_UNQUOTED(CYGWIN,1,[This is a Cygwin system])
     AC_DEFINE_UNQUOTED(WINDOWS,1,[This is a Windows system])
     AC_CHECK_LIB(intl, gettext)
     CFLAGS="-mms-bitfields $CFLAGS"
     build_target="cygwin"
     LIBPREFIX=lib
     DLLDIR=bin
     AC_PROG_CXX
     UNIXONLY=""
     ;;
*mingw*)
     AC_DEFINE_UNQUOTED(MINGW,1,[This is a MinGW system])
	 AC_DEFINE_UNQUOTED(WINDOWS,1,[This is a Windows system])
	 AC_DEFINE_UNQUOTED(_WIN32,1,[This is a Windows system])
	 AC_CHECK_LIB(intl, gettext)
	 LDFLAGS="$LDFLAGS -Wl,--export-all-symbols"
	 LIBS="$LIBS -lws2_32 -lplibc -lgnurx -lole32"
	 CFLAGS="-mms-bitfields $CFLAGS"
	 CPPFLAGS="-D_WIN32_WINNT=0x0501 -DHAVE_STAT64=1 -D__USE_MINGW_ANSI_STDIO=1 $CPPFLAGS"
	 build_target="mingw"
	 AC_PROG_CXX
	 LIBPREFIX=lib
	 DLLDIR=bin
	 UNIXONLY=""
	 funcstocheck=""
	 native_srcdir=$(cd $srcdir; pwd -W)
	 ;;
gnu*)
	 AC_DEFINE_UNQUOTED(GNU,1,[This is a GNU system])
	 build_target="gnu"
	 UNIXONLY="#"
     ;;
*)
     AC_MSG_RESULT(Unrecognised OS $host_os)
     AC_DEFINE_UNQUOTED(OTHEROS,1,[Some strange OS])
     UNIXONLY=""
;;
esac

AC_MSG_CHECKING([for build target])
AM_CONDITIONAL(LINUX, test "$build_target" = "linux")
AM_CONDITIONAL(CYGWIN,  test "$build_target" = "cygwin")
AM_CONDITIONAL(MINGW, test "$build_target" = "mingw")
AM_CONDITIONAL(GNU, test "$build_target" = "gnu")

AC_MSG_RESULT([$build_target])
AC_SUBST(build_target)

# functions
AC_CHECK_FUNCS([memset munmap strcspn])

# headers
AC_CHECK_HEADERS([fcntl.h],,AC_MSG_ERROR([Compiling libjssql requires standard UNIX headers files]))

# inline check
AC_C_INLINE

if test "$ac_cv_c_inline" != no ; then
  AC_DEFINE(HAVE_INLINE, 1, [Have inline functions])
  AC_SUBST(HAVE_INLINE)
fi

AC_FUNC_MALLOC
AC_FUNC_MMAP
AC_TYPE_OFF_T
AC_TYPE_SIZE_T
AC_TYPE_UINT32_T

##########project dependencies##########
AC_MSG_RESULT([checking for project dependencies])

# libmozjs
mozjs=false
AC_MSG_CHECKING(for mozjs)
AC_ARG_WITH(mozjs,
  [  --with-mozjs=PFX       base of mozjs installation],
  [AC_MSG_RESULT(mozjs path = "$with_mozjs")
   case $with_mozjs in
   no)
     ;;
   yes)
    AC_CHECK_HEADERS(js-17.0/jsapi.h,
     mozjs=true)
     ;;
   *)
    LDFLAGS="-L$with_mozjs/lib $LDFLAGS"
    CFLAGS="-I$with_mozjs/include/js-17.0 $CFLAGS"
 	LIBS="-lmozjs-17.0 $LIBS"   
	AC_CHECK_HEADERS(js-17.0/jsapi.h,
     mozjs=true)
    ;;
   esac
  ],
  [AC_MSG_RESULT([--with-mozjs not specified])
   LDFLAGS="-L/usr/local/lib $LDFLAGS"
   LIBS="-lmozjs-17.0 $LIBS"
   CFLAGS="-I/usr/local/include/js-17.0 $CFLAGS"
   AC_CHECK_HEADERS(js-17.0/jsapi.h, 
   mozjs=true   
   )])
AM_CONDITIONAL(HAVE_JS_17_0_JSAPI_H, test x$mozjs = xtrue)

if test "x$mozjs" != "xtrue"; then
  AC_MSG_ERROR([you need libmozjs-17.0 in order to build this project])
fi

if test "$build_target" = "mingw"
then
  CYGWIN_MYSQL_MAGIC="#include <mysql/my_global.h>"
fi

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

# postgres
if test "x$mozjs" = "x1"
then
  AM_CONDITIONAL(HAVE_JS_17_0_JSAPI_H, true)
  AC_DEFINE([HAVE_JS_17_0_JSAPI_H],[1],[Have Mozilla SpiderMonkey engine])
fi

AC_CONFIG_FILES([
Makefile
src/Makefile
])
AC_OUTPUT
