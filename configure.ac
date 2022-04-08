AC_PREREQ(2.61)
# Checks for programs.
AC_INIT([libjssql], [1.1], [support@mindbit.ro])

# Detect missing Autoconf Archive library in the system
m4_pattern_forbid([^AX_])

AC_CANONICAL_TARGET
AC_CANONICAL_HOST
AC_CANONICAL_SYSTEM
AX_COMPILER_VENDOR
AX_COMPILER_VERSION

AM_INIT_AUTOMAKE
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_FILES([Makefile src/Makefile])
AC_PROG_CC
AC_PROG_INSTALL
AC_PROG_LN_S
AC_PROG_MAKE_SET
AM_PROG_CC_C_O
AC_CHECK_SIZEOF([void *])
LT_INIT

CFLAGS="-Wall $CFLAGS"

#
# additional configure arguments
#

AC_ARG_WITH(duktape,
    AC_HELP_STRING([--with-duktape=DIR], [search for Duktape headers in DIR]),
    [
        DUKTAPE="$withval"
    ]
)

AC_ARG_WITH(jsmisc,
    AC_HELP_STRING([--with-jsmisc=DIR], [search for jsmisc in DIR]),
    [
        JSMISC="$withval"
    ]
)

#
# test for Duktape
#

if test -n "$DUKTAPE" ; then
    CPPFLAGS="-I$DUKTAPE $CPPFLAGS"
fi
AC_CHECK_HEADER(duktape.h,[],[],-)
if test "x$ac_cv_header_duktape_h" != xyes ; then
    AC_MSG_ERROR([duktape.h not found])
fi
LIBS="-lm $LIBS"
AC_CHECK_LIB([duktape],[duk_create_heap],[],[
    AC_MSG_ERROR([Duktape library not found])
])

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

AC_CHECK_LIB([jsmisc],[js_log_impl],[],[
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
if test x$found_mysql = xyes ; then
    OLD_CFLAGS="$CFLAGS"
    CFLAGS="$CFLAGS $MYSQL_CFLAGS"
    AC_CHECK_TYPE([my_bool],[],[
        AC_DEFINE_UNQUOTED([my_bool],[bool],
                           [Define to `bool' if <mysql.h> does not define.])
    ],[
#include <mysql.h>
])
    CFLAGS="$OLD_CFLAGS"
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
# Git versioning
#

AC_DEFINE_UNQUOTED(VERSION_STR,
    ["libjssql `git describe` ($host, $ax_cv_c_compiler_vendor cc $ax_cv_c_compiler_version, `expr $ac_cv_sizeof_void_p \* 8`-bit)"],
    [A string containing the version number, platform, and C compiler])

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

AC_OUTPUT
