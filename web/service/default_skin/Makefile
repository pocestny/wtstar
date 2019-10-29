all: skin.o
TEMPLATES=master.tmpl home.tmpl ide.tmpl page.tmpl usage.tmpl samples.tmpl language.tmpl \
					samples_sum.tmpl samples_psum.tmpl samples_first1.tmpl samples_lrank.tmpl \
					samples_psrch.tmpl samples_hull.tmpl

CXXFLAGS+=-std=c++14 -O3 -Wall  -I /usr/local/include

skin.cc: ${TEMPLATES}
	cppcms_tmpl_cc ${TEMPLATES} -o $@ 

.PHONY : clean
clean :
	rm -rf *.cc
	rm -rf *.o
