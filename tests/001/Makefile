CC=clang
SRCDIR=../../src
CFLAGS=-g -I /usr/lib/clang/7/include -I . -I ${SRCDIR}

all: test_hash

test_hash: ${SRCDIR}/hash.c ${SRCDIR}/hash.h test_hash.c 
	${CC} ${CFLAGS} ${SRCDIR}/hash.c test_hash.c -o test_hash


