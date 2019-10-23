all:
	make -C ide
	make -C home
	make -C service
	cp -r ./static/* ../_build/web

clean:
	make -C ide clean
	make -C home clean
	make -C service clean
