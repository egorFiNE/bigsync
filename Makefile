VERSION=0.1.2
CC=gcc -Wall -O3 -funroll-loops -D_DARWIN_FEATURE_64_BIT_INODE -D_FILE_OFFSET_BITS=64

all: bigsync

dev: bigsync 

bigsync: bigsync.o md4.o hr.o
	$(CC) -o bigsync bigsync.o md4.o hr.o

bigsync.o: bigsync.c
	$(CC) -c bigsync.c -DVERSION=\"$(VERSION)\"

md4.o: md4.c md4.h
	$(CC) -c md4.c

hr.o: hr.c hr.h
	$(CC) -c hr.c

test: test.c md4.c
	$(CC) -o test test.c md4.o
	./test

clean:
	rm -f bigsync test *.o

install: bigsync
	strip bigsync
	cp bigsync /usr/local/bin/
	mkdir -p /usr/local/man/man1/
	cp bigsync.1 /usr/local/man/man1/

dist:
	mkdir bigsync-$(VERSION)
	cp -R *.c *.h bigsync.1 Makefile README.md LICENSE bigsync-$(VERSION)/
	tar cfz bigsync-$(VERSION).tar.gz bigsync-$(VERSION)/
