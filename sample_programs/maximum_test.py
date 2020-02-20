#!/usr/bin/env python3
import random as rand
import subprocess as su
import sys

import matplotlib as mp
mp.use('Agg')
import matplotlib.pyplot as plt

def unique_list(n):
    perm = rand.sample(range(n),k=n)
    cl=[0]*n
    val=0
    for i in range(n):
        val=val+int(rand.random()*10+1)
        cl[perm[i]]=val
    return cl


def test(n):
    rep=10
    t=0
    w=0
    for t in range(rep):
        ul = unique_list(n)
        input_string = '[ '+' '.join(str(elem) for elem in ul)+' ]'
        o = su.check_output("wtrun maximum_doubly_log.wtr".split(),input=input_string, encoding='ascii').split('\n')
        if max(ul)!=int(o[0]):
            print(input_string,max(ul),int(o[0]))
        w+=float(o[1].split()[1])
        t+=float(o[1].split()[2])
    return w/rep,t/rep


if __name__ == '__main__':
    su.Popen('wtc maximum_doubly_log.wt -o maximum_doubly_log.wtr'.split(), stdout=su.DEVNULL)
    n=int(sys.argv[1])
    W=[]
    T=[]
    N=[]
    for i in range(n):
        print(i)
        N.append(i+10)
        w,t=test(i+10)
        W.append(w)
        T.append(t)
    fig,(ax1, ax2) = plt.subplots(nrows=2)
    ax1.plot(N,T)
    ax1.set_title('time')
    ax1.set_xlabel('n')
    ax2.plot(N,W)
    ax2.set_title('work')
    ax2.set_xlabel('n')
    plt.subplots_adjust(hspace=1)
    plt.savefig('maximum_test.png')
    plt.show()


