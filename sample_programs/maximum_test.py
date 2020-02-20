#!/usr/bin/env python3
import random as rand
import subprocess as su
import sys

def unique_list(n):
    perm = rand.sample(range(n),k=n)
    cl=[0]*n
    val=0
    for i in range(n):
        val=val+int(rand.random()*10+1)
        cl[perm[i]]=val
    return cl


if __name__ == '__main__':
    su.Popen('wtc maximum_doubly_log.wt -o maximum_doubly_log.wtr'.split(), stdout=su.DEVNULL)
    n=int(sys.argv[1])
    for t in range(1000):
        ul = unique_list(n)
        input_string = '[ '+' '.join(str(elem) for elem in ul)+' ]'
        o = su.check_output("wtrun maximum_doubly_log.wtr".split(),input=input_string, encoding='ascii').split('\n')
        if max(ul)!=int(o[0]):
            print(input_string,max(ul),int(o[0]))

