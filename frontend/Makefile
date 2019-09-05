SHELL := /bin/bash

CFLAGS=-I ../src 
EMCCFLAGS=-g -s EXTRA_EXPORTED_RUNTIME_METHODS="['cwrap']" -s EXPORT_ES6=1 -s MODULARIZE=1 \
					 -s ALLOW_MEMORY_GROWTH=1 -s NO_FILESYSTEM=1 
			
BACKENDSRC=ast.c parser.c scanner.c driver.c writer.c code_generation.c errors.c runtime.c reader.c vm.c instr_names.c
CSRC=backend.c $(foreach file,${BACKENDSRC},../src/${file})

EXPORTS=['_web_compile','_errnum','_get_error_msg','_web_set_input','_web_run', '_web_output']

all: install

install:
	make -C ../src parser.c
	source /home/kralovic/SW/emsdk/emsdk_env.sh; \
		emcc  ${CFLAGS} ${EMCCFLAGS} -s EXPORTED_FUNCTIONS="${EXPORTS}"  ${CSRC}\
		-o backend.js
	./rollup --config
	mv index.js backend.wasm index.css ../_build/frontend
	cp -R ./static/* ../_build/frontend

clean:
	rm -f index.js backend.js backend.wasm
