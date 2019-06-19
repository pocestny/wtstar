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

WTC_SRC=parser.c driver.c parser_utils.c
WTC_HDRS=ast.h code.h utils.h parser_utils.h parser.c
WTC_DEPS=${WTC_SRC} ${WTC_HDRS}

all: wtc 

.PHONY:	wtc
wtc: ${BUILD_DIR}/cli/wtc


${BUILD_DIR}/wtc: ${WTC_DEPS}
	mkdir -p ${BUILD_DIR}/cli
	${CC} ${CFLAGS} ${WTC_SRC} -o ${BUILD_DIR}/cli/wtc

%.c: %.y
	bison ${BISONFLAGS} -o $@ $<

%.c: %.l
	flex --header-file=`basename $< .l`.h -o $@ $<

clean:
	echo "."
