AC_PREREQ(2.61)
# Checks for programs.
AC_INIT([libjssql], [0.0.1], [support@mindbit.ro])

# Detect missing Autoconf Archive library in the system
m4_pattern_forbid([^AX_])

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
LT_INIT
AC_SUBST(MKDIR_P)


CFLAGS="-Wall $CFLAGS"

#
# additional configure arguments
#

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

#
# test for SpiderMonkey 17.0 (mozjs 17.0)
#

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

#
# test for jsmisc
#

if test -n "$JSMISC" ; then
    CPPFLAGS="-I$JSMISC $CPPFLAGS"
fi
AC_CHECK_HEADER(jsmisc.h,[],[],-)
if test "x$ac_cv_header_jsmisc_h" != xyes ; then
    AC_MSG_ERROR([jsmisc.h not found])
fi

AC_CHECK_LIB([jsmisc],[JS_LogImpl],[],[
    AC_MSG_ERROR([jsmisc library not found])
])

#
# test for mysql
#

AX_LIB_MYSQL([4.1.0])
AM_CONDITIONAL(HAVE_MYSQL, test x$found_mysql = xyes)
if test -z "$found_mysql" ; then
    found_mysql=no
fi

#
# test for postgres
#

AX_LIB_POSTGRESQL
AM_CONDITIONAL(HAVE_POSTGRESQL, test x$found_postgresql = xyes)
if test -z "$found_postgresql" ; then
    found_postgresql=no
fi

#
# Debug mode
#

AC_ARG_ENABLE([debug],
    AS_HELP_STRING([--enable-debug], [Enable debugging mode]))

AS_IF([test "x$enable_debug" = "xyes"], [
    CFLAGS="$CFLAGS -DJS_DEBUG"
], [
    enable_debug=no
])

#
# Configuration summary
#

echo >&AS_MESSAGE_FD
echo "Configuration summary" >&AS_MESSAGE_FD
echo "---------------------" >&AS_MESSAGE_FD
echo "MySQL support ........ $found_mysql " >&AS_MESSAGE_FD
echo "PostgreSQL support ... $found_postgresql" >&AS_MESSAGE_FD
echo "Debugging support .... $enable_debug" >&AS_MESSAGE_FD
echo >&AS_MESSAGE_FD

AC_CONFIG_FILES([
Makefile
src/Makefile
])
AC_OUTPUT
