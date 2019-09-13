CC=clang
SRCDIR=../../src
CFLAGS=-g -I /usr/lib/clang/7/include -I . -I ${SRCDIR}

all: test_path

test_path: ${SRCDIR}/path.c test_path.c ${SRCDIR}/path.h
	${CC} ${CFLAGS} ${SRCDIR}/path.c test_path.c -o test_path


