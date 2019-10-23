all: cli-tools web documentation

.PHONY:	cli-tools web documentation

cli-tools:
	make -C src

web:
	make -C web

documentation:
	make -C src documentation

clean:
	make -C src clean
	make -C web clean
