.PHONY: all clean

CFLAGS = -I/usr/include/js -Wall
LDFLAGS = -ljs

all: test

test: test.o js_sql.o

clean:
	rm -f *.o test
