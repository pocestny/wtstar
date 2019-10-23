all:
	${MAKE} -C ide
	${MAKE} -C home
	${MAKE} -C service
	cp -r ./static/* ../_build/web

clean:
	${MAKE} -C ide clean
	${MAKE} -C home clean
	${MAKE} -C service clean
