#!/usr/bin/env python3
import random as rand
import subprocess as su
import sys

def wrap(A):
    return '[ '+' '.join(str(elem) for elem in A)+' ] '

if __name__ == '__main__':
    su.Popen('wtc merge_log.wt -o merge_log.wtr'.split(), stdout=su.DEVNULL)
    n=int(sys.argv[1])
    for t in range(1000):
        all = rand.sample(range(20*n),k=2*n)
        A = all[0:n]
        B = all[n:]
        input_string = wrap(sorted(A)) + wrap(sorted(B))
        o = su.check_output("wtrun merge_log.wtr".split(),input=input_string, encoding='ascii').split('\n')
        res = list(map(int,o[0].translate({ord(c): None for c in '[]'}).split(' '))) 
        if res!=sorted(all):
            print (input_string)
