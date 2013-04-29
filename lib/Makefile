.PHONY: all clean

#JSDIR=/opt/js-17.0.0-debug
#CFLAGS = -I$(JSDIR)/include/js -Wall -g -O0 -DDEBUG
#LDFLAGS = -L$(JSDIR)/lib -Wl,-rpath,$(JSDIR)/lib -lmozjs-17.0

CFLAGS = -I/usr/include/js -Wall -g -O0
LDFLAGS = -ljs

all: test

test: test.o js_sql.o js_mysql.o

clean:
	rm -f *.o test
