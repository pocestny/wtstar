#!/usr/bin/env python3
import random as rand
import subprocess as su
import sys

def circ_list(n):
    perm = rand.sample(range(n),k=n)
    cl=[0]*n
    for i in range(n):
        cl[perm[i]]=perm[(i+1)%n]
    return cl


def check(S,C):
    nc = 0
    ok = 1
    for i in range(len(S)):
        if C[i]==C[S[i]]:
            ok=0
            print(i,C[i],S[i],C[S[i]])
        if C[i]>nc:
            nc=C[i]
    nc = nc+1
    print(ok,nc)

if __name__ == '__main__':
    su.Popen('wtc coloring_fast.wt -o coloring_fast.wtr'.split(), stdout=su.DEVNULL)
    n=int(sys.argv[1])
    for t in range(1000):
        cl = circ_list(n)
        #print(cl)
        #print('[',*cl,']',sep=' ')
        input_string = '[ '+' '.join(str(elem) for elem in cl)+' ]'
        #print(input_string)
        o = su.check_output("wtrun coloring_fast.wtr".split(),input=input_string, encoding='ascii').split('\n')
        c = list(map(lambda x:int(x),list(filter(lambda x:x.isdigit() , o[0]))))
        #print(c)
        check(cl,c)
