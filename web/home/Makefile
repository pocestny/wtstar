DSTDIR=../../_build/web

FRONTENDSRC=babel.config.js home.scss package.json rollup.config.js home.js 

.PHONY: clean all-js

all: all-js

${DSTDIR}:
	mkdir -p ${DSTDIR}

all-js: ${FRONTENDSRC} 
	./rollup --config


clean:
	rm -rf ${DSTDIR}





