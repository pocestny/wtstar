SHELL := /bin/bash

CLIDIR=../../src
DSTDIR=../../_build/web

CFLAGS=-I ${CLIDIR}
EMCCFLAGS=-g -s EXTRA_EXPORTED_RUNTIME_METHODS="['cwrap','getValue']" -s EXPORT_ES6=1 \
					-s MODULARIZE=1 -s ALLOW_MEMORY_GROWTH=1 -s NO_FILESYSTEM=1 
			
BACKENDSRC=ast.c parser.c scanner.c driver.c writer.c code_generation.c \
					 errors.c reader.c vm.c instr_names.c hash.c path.c \
					 debug.c web_interface.c

BACKENDHDR=ast.h parser.y scanner.l driver.h writer.h code_generation.h errors.h\
					 reader.h vm.h hash.h path.h debug.h

CSRC=$(foreach file,${BACKENDSRC},${CLIDIR}/${file})

EXPORTS=['_web_compile','_errnum','_get_error_msg','_web_run', '_web_output', \
				'_web_W','_web_T','_web_start', '_web_stop','_web_state','_web_name', \
				'_web_tids','_web_n_threads','_web_thread_parent','_web_thread_base_name',\
				'_web_thread_base_value','_web_prepare_vars','_web_var_shared',\
				'_web_var_type','_web_var_name','_web_var_dims','_web_var_value',\
				'_web_current_line']

FRONTENDSRC=babel.config.js editor.js ide.scss get_string.form.txt get_string.js \
						iopanel.js logger.js main.js package.json rollup.config.js statusbar.js \
						watcher.js

.PHONY: clean all-js

all: all-js

${DSTDIR}:
	mkdir -p ${DSTDIR}

${DSTDIR}/backend.js:  ${DSTDIR} ${CSRC} $(foreach file,${BACKENDHDR},${CLIDIR}/${file})
	export EMCC_WASM_BACKEND=1;\
		emcc  ${CFLAGS} ${EMCCFLAGS} -s EXPORTED_FUNCTIONS="${EXPORTS}"  ${CSRC}\
		-o $@

all-js: ${FRONTENDSRC} ${DSTDIR}/backend.js
	./rollup --config

%.c: %.y
	bison  -o $@ $<

%.c: %.l
	flex --header-file=`dirname $<`/`basename $< .l`.h -o $@ $<


clean:
	rm -rf ${DSTDIR}





