DEBUG=0

CC=clang
BUILD_DIR=../_build
CFLAGS=-I /usr/lib/clang/7/include -I .

ifeq ($(DEBUG),1)
CFLAGS+=-g
BISONFLAGS= -v -fdiagnostics-show-caret -fdiagnostics-parseable-fixit
else
CFLAGS+=-O3 -static
BISONFLAGS=
endif

.PHONY:	wtc wtrun wtdump wtdb documentation
all: wtc wtrun wtdump wtdb 

documentation:
	mkdir -p ${BUILD_DIR}/documentation
	doxygen

##################################################################
########  build wtc
WTC_SRC = ast.c parser.c scanner.c driver.c writer.c wtc.c \
					ast_debug_print.c code_generation.c errors.c path.c \
					debug.c hash.c

WTC_HDRS= ast.h parser.h scanner.h driver.h writer.h code.h\
					utils.h ast_debug_print.h code_generation.h errors.h \
					path.h debug.h hash.h

WTC_DEPS=${WTC_SRC} ${WTC_HDRS} parser_utils.c


##################################################################
########  build wtrun
WTR_SRC = wtrun.c vm.c instr_names.c reader.c writer.c  \
					errors.c hash.c debug.c

WTR_HDRS= code.h vm.h reader.h writer.h  errors.h hash.h debug.h

WTR_DEPS=${WTR_SRC} ${WTR_HDRS} 

##################################################################
########  build wtdb
WTDB_SRC = wtdb.c vm.c instr_names.c reader.c writer.c  \
					errors.c hash.c debug.c linenoise.c

WTDB_HDRS= code.h vm.h reader.h writer.h  errors.h hash.h debug.h \
					 linenoise.h

WTDB_DEPS=${WTDB_SRC} ${WTDB_HDRS} 

##################################################################
########  build wtdump
WTDUMP_SRC = wtdump.c instr_names.c reader.c writer.c  \
						 errors.c hash.c debug.c vm.c

WTDUMP_HDRS= code.h reader.h writer.h  vm.h errors.h hash.h \
						 debug.h

WTDUMP_DEPS=${WTDUMP_SRC} ${WTDUMP_HDRS} 

##################################################################

wtc: ${BUILD_DIR}/cli_tools/wtc
wtrun: ${BUILD_DIR}/cli_tools/wtrun
wtdb: ${BUILD_DIR}/cli_tools/wtdb
wtdump: ${BUILD_DIR}/cli_tools/wtdump


${BUILD_DIR}/cli_tools/wtc: ${WTC_DEPS}
	mkdir -p ${BUILD_DIR}/cli_tools
	${CC} ${CFLAGS} ${WTC_SRC} -o ${BUILD_DIR}/cli_tools/wtc

${BUILD_DIR}/cli_tools/wtrun: ${WTR_DEPS}
	mkdir -p ${BUILD_DIR}/cli_tools
	${CC} ${CFLAGS} ${WTR_SRC} -o ${BUILD_DIR}/cli_tools/wtrun -lm

${BUILD_DIR}/cli_tools/wtdb: ${WTDB_DEPS}
	mkdir -p ${BUILD_DIR}/cli_tools
	${CC} ${CFLAGS} ${WTDB_SRC} -o ${BUILD_DIR}/cli_tools/wtdb -lm 

${BUILD_DIR}/cli_tools/wtdump: ${WTDUMP_DEPS}
	mkdir -p ${BUILD_DIR}/cli_tools
	${CC} ${CFLAGS} ${WTDUMP_SRC} -o ${BUILD_DIR}/cli_tools/wtdump -lm 

%.c: %.y
	bison ${BISONFLAGS} -o $@ $<

%.c: %.l
	flex --header-file=`basename $< .l`.h -o $@ $<

clean:
	rm -rf parser.output parser.c parser.h scanner.c scanner.h
