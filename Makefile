UNAME_S=$(shell uname -s)
UNAME_N=$(shell uname -n)

$(info system is [${UNAME_S}] [${UNAME_N}])

ifeq ($(UNAME_S),FreeBSD)
export MAKE=gmake
endif

all: cli-tools web documentation

.PHONY:	cli-tools web documentation

cli-tools:
	${MAKE} -C src

web:
	${MAKE} -C web

documentation:
	${MAKE} -C src documentation

clean:
	${MAKE} -C src clean
	${MAKE} -C web clean
