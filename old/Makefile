SHELL := /bin/bash

CFLAGS=-I ../src 
EMCCFLAGS=-g -s EXTRA_EXPORTED_RUNTIME_METHODS="['cwrap','getValue']" -s EXPORT_ES6=1 \
					-s MODULARIZE=1 -s ALLOW_MEMORY_GROWTH=1 -s NO_FILESYSTEM=1 
			
BACKENDSRC=ast.c parser.c scanner.c driver.c writer.c code_generation.c \
					 errors.c reader.c vm.c instr_names.c hash.c path.c \
					 debug.c web_interface.c

BACKENDHDR=ast.h parser.y scanner.l driver.h writer.h code_generation.h errors.h\
					 reader.h vm.h hash.h path.h debug.h

CSRC=$(foreach file,${BACKENDSRC},../src/${file})

EXPORTS=['_web_compile','_errnum','_get_error_msg','_web_run', '_web_output', \
				'_web_W','_web_T','_web_start', '_web_stop','_web_state','_web_name', \
				'_web_tids','_web_n_threads','_web_thread_parent','_web_thread_base_name',\
				'_web_thread_base_value','_web_prepare_vars','_web_var_shared',\
				'_web_var_type','_web_var_name','_web_var_dims','_web_var_value',\
				'_web_current_line']

FRONTENDSRC=babel.config.js editor.js frontend.scss get_string.form.txt get_string.js \
						iopanel.js logger.js main.js package.json rollup.config.js statusbar.js \
						watcher.js

.PHONY: clean static doxygen

all: lock.package

lock.backend: ${CSRC} $(foreach file,${BACKENDHDR},../src/${file})
	touch lock.backend
	make -C ../src parser.c
	source /home/kralovic/SW/emsdk/emsdk_env.sh; export EMCC_WASM_BACKEND=1;\
		emcc  ${CFLAGS} ${EMCCFLAGS} -s EXPORTED_FUNCTIONS="${EXPORTS}"  ${CSRC}\
		-o backend.js

lock.frontend: lock.backend ${FRONTENDSRC}
	touch lock.frontend
	./rollup --config
	mkdir -p ../_build/frontend
	cp frontend.js backend.wasm frontend.css ../_build/frontend

doxygen:
	cd ..; doxygen

lock.documentation: ../README.md index_header ./static/markedapp-byword.css doxygen
	touch lock.documentation
	mkdir -p ../_build/frontend
	cat ../README.md | sed -e 's/frontend\/static\///g' | pandoc -M pagetitle="WT* framework"  \
		--include-in-header='index_header' -f markdown -t html -c ./markedapp-byword.css -s  \
		-o ../_build/frontend/index.html

static:
	mkdir -p ../_build/frontend
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
