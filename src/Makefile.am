lib_LTLIBRARIES = libjssql.la

libjssql_la_SOURCES = \
  js_sql.c \
  js_utils.c

if HAVE_POSTGRESQL_LIBPQ_FE_H
  libjssql_la_SOURCES += js_postgres.c
endif

if HAVE_MYSQL_MYSQL_H
  libjssql_la_SOURCES += js_mysql.c
endif
libjssql_la_LIBADD = \
  $(LIBS)

libjssql_la_LDFLAGS = \
  $(LDFLAGS) \
  -version-info 11:0:2

check_PROGRAMS = \
  test

test_SOURCES = \
  test.c
test_LDADD = \
  libjssql.la
test_DEPENDENCIES = \
  libjssql.la
