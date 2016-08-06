#
# Main library
#

lib_LTLIBRARIES = libjssql.la
libjssql_la_SOURCES = js_sql.c js_common.c
libjssql_la_LIBADD = $(LIBS)
libjssql_la_LDFLAGS = $(LDFLAGS) -version-info 1
include_HEADERS = js_sql.h

#
# MySQL support
#

if HAVE_MYSQL
    lib_LTLIBRARIES += libjssql_mysql.la
    include_HEADERS += js_mysql.h
endif

libjssql_mysql_la_SOURCES = js_mysql.c
libjssql_mysql_la_CFLAGS = @MYSQL_CFLAGS@
libjssql_mysql_la_LIBADD = @MYSQL_LDFLAGS@
libjssql_mysql_la_LDFLAGS = -version-info 1

#
# PostgreSQL support
#

if HAVE_POSTGRESQL
    lib_LTLIBRARIES += libjssql_postgres.la
    include_HEADERS += js_postgres.h
endif

libjssql_postgres_la_SOURCES = js_postgres.c
libjssql_postgres_la_CFLAGS = @POSTGRESQL_CPPFLAGS@
libjssql_postgres_la_LIBADD = @POSTGRESQL_LIBS@
libjssql_postgres_la_LDFLAGS = @POSTGRESQL_LDFLAGS@ -version-info 1

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
    test_LDADD += libjssql_postgres.la
    test_DEPENDENCIES += libjssql_postgres.la
endif
