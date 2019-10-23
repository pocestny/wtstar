BINARY=../../_build/wtstar

include ../credentials

COMMON_CXXFLAGS=-std=c++14 -g -DDATABASE=\"${DATABASE}\" -DROOT_DIR=\"${ROOT_DIR}\" -DSTATIC_PREFIX=\"${STATIC_PREFIX}\" -DVERSION=\"${VERSION}\" -Wall -I /usr/local/include
COMMON_LDFLAGS=-L /usr/local/lib 

UNAME_S=$(shell uname -s)
UNAME_N=$(shell uname -n)

$(info system is [${UNAME_S}] [${UNAME_N}])

ifeq ($(UNAME_S),FreeBSD)
export MAKE=gmake
export CXX=clang++
export CXXFLAGS = -stdlib=libc++ ${COMMON_CXXFLAGS} -D_Bool=bool
export LDFLAGS = ${COMMON_LDFLAGS} -lc++
endif

ifeq  ($(UNAME_S),Linux)
export CXX=clang++
export CXXFLAGS = ${COMMON_CXXFLAGS}
export LDFLAGS = ${COMMON_LDFLAGS}
endif


CXXFLAGS += -DUSE_CPPCMS

SKIN_DIR=default_skin
OBJS=${SKIN_DIR}/skin.o 
SOURCES=wtstar.cc
DEPS=${SOURCES}  content.h ${CREDENTIALS}

all: skin ${BINARY} 

.PHONY:	snippets.cc
snippets.cc: 
	./create_snippets.sh

${BINARY}: ${OBJS} ${DEPS} snippets.cc
	$(CXX) ${CXXFLAGS} -o $@ ${SOURCES} ${OBJS} ${LDFLAGS} -lcppcms -lbooster 

.PHONY: clean skin

skin:
	$(MAKE) -C ${SKIN_DIR}

clean:
	rm -fr ${BINARY} 
	rm -fr *.o
	$(MAKE) clean -C ${SKIN_DIR}


#### not needed so far, but maybe later

%.h: %.svg
	echo -n 'std::string ' `basename $< .svg`_svg ' = R"~~~~( ' > $@
	cat $< >> $@
	echo ')~~~~";' >> $@


