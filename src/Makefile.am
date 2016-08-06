#
# Main library
#

lib_LTLIBRARIES = libjssql.la
libjssql_la_SOURCES = jssql.c jscommon.c
libjssql_la_LIBADD = $(LIBS)
libjssql_la_LDFLAGS = $(LDFLAGS) -version-info 1
include_HEADERS = jssql.h

#
# MySQL support
#

if HAVE_MYSQL
    lib_LTLIBRARIES += libjssql_mysql.la
    include_HEADERS += jsmysql.h
endif

libjssql_mysql_la_SOURCES = jsmysql.c
libjssql_mysql_la_CFLAGS = @MYSQL_CFLAGS@
libjssql_mysql_la_LIBADD = @MYSQL_LDFLAGS@
libjssql_mysql_la_LDFLAGS = -version-info 1

#
# PostgreSQL support
#

if HAVE_POSTGRESQL
    lib_LTLIBRARIES += libjssql_pgsql.la
    include_HEADERS += jspgsql.h
endif

libjssql_pgsql_la_SOURCES = jspgsql.c
libjssql_pgsql_la_CFLAGS = @POSTGRESQL_CPPFLAGS@
libjssql_pgsql_la_LIBADD = @POSTGRESQL_LIBS@
libjssql_pgsql_la_LDFLAGS = @POSTGRESQL_LDFLAGS@ -version-info 1

#
# Test programs
#

check_PROGRAMS = test
test_SOURCES = test.c
test_LDADD = libjssql.la
test_DEPENDENCIES = libjssql.la

if HAVE_MYSQL
    test_LDADD += libjssql_mysql.la
    test_DEPENDENCIES += libjssql_mysql.la
endif

if HAVE_POSTGRESQL
    test_LDADD += libjssql_pgsql.la
    test_DEPENDENCIES += libjssql_pgsql.la
endif
