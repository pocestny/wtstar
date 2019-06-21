DEBUG=1

CC=clang
BUILD_DIR=../_build
CFLAGS=-I /usr/lib/clang/7/include


ifeq ($(DEBUG),1)
CFLAGS+=-g
BISONFLAGS=-v -fdiagnostics-show-caret -fdiagnostics-parseable-fixit
else
CFLAGS+=-O3
BISONFLAGS=
endif

WTC_SRC = ast.c parser.c scanner.c driver.c writer.c wtc.c
WTC_HDRS= ast.h parser.h scanner.h driver.h writer.h code.h utils.h
WTC_DEPS=${WTC_SRC} ${WTC_HDRS} parser_utils.c

all: wtc 

.PHONY:	wtc
wtc: ${BUILD_DIR}/cli/wtc


${BUILD_DIR}/cli/wtc: ${WTC_DEPS}
	mkdir -p ${BUILD_DIR}/cli
	${CC} ${CFLAGS} ${WTC_SRC} -o ${BUILD_DIR}/cli/wtc

%.c: %.y
	bison ${BISONFLAGS} -o $@ $<

%.c: %.l
	flex --header-file=`basename $< .l`.h -o $@ $<

clean:
	rm -rf parser.output parser.c parser.h scanner.c scanner.h
