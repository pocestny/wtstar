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

WTC_SRC = ast.c parser.c scanner.c driver.c writer.c wtc.c ast_debug_print.c code_generation.c
WTC_HDRS= ast.h parser.h scanner.h driver.h writer.h code.h utils.h ast_debug_print.h code_generation.h
WTC_DEPS=${WTC_SRC} ${WTC_HDRS} parser_utils.c

WTR_SRC = wtr.c vm.c instr_names.c
WTR_HDRS= code.h vm.h
WTR_DEPS=${WTR_SRC} ${WTR_HDRS} 

all: wtc wtr

.PHONY:	wtc wtr
wtc: ${BUILD_DIR}/cli/wtc
wtr: ${BUILD_DIR}/cli/wtr


${BUILD_DIR}/cli/wtc: ${WTC_DEPS}
	mkdir -p ${BUILD_DIR}/cli
	${CC} ${CFLAGS} ${WTC_SRC} -o ${BUILD_DIR}/cli/wtc

${BUILD_DIR}/cli/wtr: ${WTR_DEPS}
	mkdir -p ${BUILD_DIR}/cli
	${CC} ${CFLAGS} ${WTR_SRC} -o ${BUILD_DIR}/cli/wtr

%.c: %.y
	bison ${BISONFLAGS} -o $@ $<

%.c: %.l
	flex --header-file=`basename $< .l`.h -o $@ $<

clean:
	rm -rf parser.output parser.c parser.h scanner.c scanner.h
