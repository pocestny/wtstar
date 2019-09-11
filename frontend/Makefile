SHELL := /bin/bash

CFLAGS=-I ../src 
EMCCFLAGS=-g -s EXTRA_EXPORTED_RUNTIME_METHODS="['cwrap']" -s EXPORT_ES6=1 -s MODULARIZE=1 \
					 -s ALLOW_MEMORY_GROWTH=1 -s NO_FILESYSTEM=1 
			
BACKENDSRC=ast.c parser.c scanner.c driver.c writer.c code_generation.c errors.c runtime.c reader.c vm.c instr_names.c hash.c
BACKENDHDR=ast.h parser.y scanner.l driver.h writer.h code_generation.h errors.h runtime.h reader.h vm.h hash.h

CSRC=backend.c $(foreach file,${BACKENDSRC},../src/${file})

EXPORTS=['_web_compile','_errnum','_get_error_msg','_web_run', '_web_output', '_web_W','_web_T','_web_start']

FRONTENDSRC=babel.config.js editor.js frontend.scss get_string.form.txt get_string.js iopanel.js logger.js main.js package.json rollup.config.js statusbar.js

.PHONY: clean static

all: lock.package

lock.backend: ${CSRC} $(foreach file,${BACKENDHDR},../src/${file})
	touch lock.backend
	make -C ../src parser.c
	source /home/kralovic/SW/emsdk/emsdk_env.sh; \
		emcc  ${CFLAGS} ${EMCCFLAGS} -s EXPORTED_FUNCTIONS="${EXPORTS}"  ${CSRC}\
		-o backend.js

lock.frontend: lock.backend ${FRONTENDSRC}
	touch lock.frontend
	./rollup --config
	cp frontend.js backend.wasm frontend.css ../_build/frontend

lock.documentation: ../README.md index_header ./static/markedapp-byword.css
	touch lock.documentation
	cat ../README.md | sed -e 's/frontend\/static\///g' | pandoc -M pagetitle="WT* framework"  --include-in-header='index_header' -f markdown -t html -c ./markedapp-byword.css -s  -o ../_build/frontend/index.html

static:
	cp -R ./static/* ../_build/frontend

lock.package: lock.backend lock.frontend lock.documentation static
	touch lock.package

lock.install: lock.package
	touch lock.install
	rsync -avz --delete ../_build/frontend/ rasto@beda.dcs.fmph.uniba.sk:/data/www/root/wt

install: lock.install
	echo '.'

clean:
	rm -f frontend.js backend.js *.wasm *.wast *.map lock.* frontend.css
